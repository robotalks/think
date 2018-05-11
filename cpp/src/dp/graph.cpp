#include <thread>
#include <stdexcept>
#include <glog/logging.h>

#include "dp/util/queue.h"
#include "dp/graph.h"

namespace dp {
    using namespace std;

    graph::graph(const string& name)
    : m_name(name) {
    }

    graph::variable* graph::def_var(const string& name) {
        auto pr = m_vars.insert(make_pair(name, nullptr));
        if (!pr.second) throw invalid_argument("variable already defined: " + name);
        auto v = new variable(name);
        pr.first->second.reset(v);
        m_var_deps.insert(make_pair(name, list<op*>()));
        return v;
    }

    void graph::def_vars(const vector<string>& names) {
        for (auto name : names) {
            def_var(name);
        }
    }

    void graph::add_op(const string& name,
        const vector<string>& inputs,
        const vector<string>& outputs,
        const op_func& fn) {
        auto pr = m_ops.insert(make_pair(name, nullptr));
        if (!pr.second) throw invalid_argument("operator already defined: " + name);
        op *o = new op(name, fn);
        for (auto& n : inputs) {
            o->params.push_back(var(n));
            m_var_deps.find(n)->second.push_back(o);
        }
        for (auto& n : outputs) {
            auto pr = m_out_vars.insert(make_pair(n, name));
            if (!pr.second)
                throw invalid_argument("var " + n + " is already output from op " + pr.first->second);
            o->results.push_back(var(n));
        }
        pr.first->second.reset(o);
    }

    graph::variable* graph::find_var(const string& name) const noexcept {
        auto it = m_vars.find(name);
        return it == m_vars.end() ? nullptr : it->second.get();
    }

    graph::variable* graph::var(const string& name) const {
        auto v = find_var(name);
        if (v == nullptr)
            throw invalid_argument("variable not found: " + name);
        return v;
    }

    void graph::reset() {
        for (auto& pr : m_vars) {
            pr.second->clear();
        }
        for (auto& pr : m_ops) {
            pr.second->set_varn = 0;
        }
    }

    void graph::exec(size_t concurrency) {
        VLOG(2) << "G:" + m_name << " EXEC";
        single_consumer_queue<op*> doneq(m_ops.size());
        queue<op*> runq;
        vector<unique_ptr<thread>> threads(concurrency);
        size_t count = 0;
        for (size_t i = 0; i < concurrency; i ++) {
            threads[i].reset(new thread([this, &doneq, &runq] () {
                op *o = nullptr;
                while ((o = runq.get()) != nullptr) {
                    VLOG(2) << "G:" + m_name << " OP:" + o->name << " START";
                    o->run([&doneq, o] () {
                        doneq.put(o);
                    });
                }
            }));
        }

        for (auto& pr : m_vars) {
            if (pr.second->is_set()) {
                auto it = m_var_deps.find(pr.first);
                if (it != m_var_deps.end()) {
                    for (auto o : it->second) {
                        o->set_varn ++;
                        if (o->activated()) {
                            count ++;
                            runq.put(o);
                        }
                    }
                }
            }
        }

        while (count > 0) {
            auto o = doneq.get();
            if (o == nullptr) break;
            VLOG(2) << "G:" + m_name << " OP:" + o->name << " DONE";
            count --;
            for (auto v : o->results) {
                if (!v->is_set())
                    throw logic_error("op " + o->name + " completes without set output " + v->name());
                const list<op*>& deps = m_var_deps[v->name()];
                for (auto d : deps) {
                    d->set_varn ++;
                    if (d->activated()) {
                        count ++;
                        runq.put(d);
                    }
                }
            }
        }

        for (size_t i = 0; i < threads.size(); i ++) runq.put(nullptr);
        for (auto& t : threads) t->join();

        VLOG(2) << "G:" + m_name << " DONE";
    }

    function<void()> graph::ctx::defer() {
        function<void()> fn = *m_done_ptr;
        if (!fn) throw logic_error("defer already called");
        *m_done_ptr = nullptr;
        return fn;
    }

    void graph::op::run(function<void()> done) {
        fn(ctx(this, &done));
        if (done) done();
    }
}
