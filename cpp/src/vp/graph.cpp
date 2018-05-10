#include <thread>
#include <stdexcept>
#include <glog/logging.h>

#include "vp/util/queue.h"
#include "vp/graph.h"

namespace vp {
    using namespace std;

    Graph::Graph(const string& name)
    : m_name(name) {
    }

    Graph::Var* Graph::defVar(const string& name) {
        auto pr = m_vars.insert(make_pair(name, nullptr));
        if (!pr.second) throw invalid_argument("variable already defined: " + name);
        auto v = new Var(name);
        pr.first->second.reset(v);
        m_var_deps.insert(make_pair(name, list<Op*>()));
        return v;
    }

    void Graph::defVars(const vector<string>& names) {
        for (auto name : names) {
            defVar(name);
        }
    }

    void Graph::addOp(const string& name,
        const vector<string>& inputs,
        const vector<string>& outputs,
        const OpFunc& fn) {
        auto pr = m_ops.insert(make_pair(name, nullptr));
        if (!pr.second) throw invalid_argument("operator already defined: " + name);
        Op *o = new Op(name, fn);
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

    Graph::Var* Graph::findVar(const string& name) const noexcept {
        auto it = m_vars.find(name);
        return it == m_vars.end() ? nullptr : it->second.get();
    }

    Graph::Var* Graph::var(const string& name) const {
        auto v = findVar(name);
        if (v == nullptr)
            throw invalid_argument("variable not found: " + name);
        return v;
    }

    void Graph::reset() {
        for (auto& pr : m_vars) {
            pr.second->clear();
        }
        for (auto& pr : m_ops) {
            pr.second->set_varn = 0;
        }
    }

    void Graph::exec(size_t concurrency) {
        VLOG(2) << "G:" + m_name << " EXEC";
        SingleConsumerQueue<Op*> doneq(m_ops.size());
        Queue<Op*> runq;
        vector<unique_ptr<thread>> threads(concurrency);
        size_t count = 0;
        for (size_t i = 0; i < concurrency; i ++) {
            threads[i].reset(new thread([this, &doneq, &runq] () {
                Op *op = nullptr;
                while ((op = runq.get()) != nullptr) {
                    VLOG(2) << "G:" + m_name << " OP:" + op->name << " START";
                    op->run([&doneq, op] () {
                        doneq.put(op);
                    });
                }
            }));
        }

        for (auto& pr : m_vars) {
            if (pr.second->is_set()) {
                auto it = m_var_deps.find(pr.first);
                if (it != m_var_deps.end()) {
                    for (auto op : it->second) {
                        op->set_varn ++;
                        if (op->activated()) {
                            count ++;
                            runq.put(op);
                        }
                    }
                }
            }
        }

        while (count > 0) {
            auto op = doneq.get();
            if (op == nullptr) break;
            VLOG(2) << "G:" + m_name << " OP:" + op->name << " DONE";
            count --;
            for (auto v : op->results) {
                if (!v->is_set())
                    throw logic_error("op " + op->name + " completes without set output " + v->name());
                const list<Op*>& deps = m_var_deps[v->name()];
                for (auto o : deps) {
                    o->set_varn ++;
                    if (o->activated()) {
                        count ++;
                        runq.put(o);
                    }
                }
            }
        }

        for (size_t i = 0; i < threads.size(); i ++) runq.put(nullptr);
        for (auto& t : threads) t->join();

        VLOG(2) << "G:" + m_name << " DONE";
    }

    function<void()> Graph::Ctx::defer() {
        function<void()> fn = *m_done_ptr;
        if (!fn) throw logic_error("defer already called");
        *m_done_ptr = nullptr;
        return fn;
    }

    void Graph::Op::run(function<void()> done) {
        fn(Ctx(this, &done));
        if (done) done();
    }
}
