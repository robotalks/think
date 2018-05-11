#include <stdexcept>

#include "dp/graph.h"

namespace dp {
    using namespace std;

    graph::builder& graph::builder::var(const string& name) {
        m_vars.insert(name);
        return *this;
    }

    graph::builder& graph::builder::vars(const vector<string>& names) {
        for (auto& name : names)
            m_vars.insert(name);
        return *this;
    }

    graph::builder& graph::builder::op(const string& name,
        const vector<string>& inputs,
        const vector<string>& outputs,
        const graph::op_factory_func& factory) {
        for (auto& n : inputs) {
            if (m_vars.find(n) == m_vars.end())
                throw invalid_argument("undef var: " + n);
        }
        for (auto& n : outputs) {
            if (m_vars.find(n) == m_vars.end())
                throw invalid_argument("undef var: " + n);
            auto pr = m_out_vars.insert(make_pair(n, name));
            if (!pr.second)
                throw invalid_argument("var " + n + " is already output from op " + pr.first->second);
        }
        op_def def;
        def.name = name;
        def.inputs = inputs;
        def.outputs = outputs;
        def.factory = factory;
        auto pr = m_ops.insert(make_pair(name, def));
        if (!pr.second)
            throw invalid_argument("op already defined: " + name);
        return *this;
    }

    graph* graph::builder::build(const string& name) const {
        graph* g = new_graph(name);
        for (auto& name : m_vars) {
            g->def_var(name);
        }
        for (auto& pr : m_ops) {
            g->add_op(pr.second.name,
                pr.second.inputs, pr.second.outputs,
                pr.second.factory());
        }
        return g;
    }
}
