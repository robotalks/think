#include <stdexcept>

#include "vp/graph.h"

namespace vp {
    using namespace std;

    Graph::Builder& Graph::Builder::var(const string& name) {
        m_vars.insert(name);
        return *this;
    }

    Graph::Builder& Graph::Builder::vars(const vector<string>& names) {
        for (auto& name : names)
            m_vars.insert(name);
        return *this;
    }

    Graph::Builder& Graph::Builder::op(const string& name,
        const vector<string>& inputs,
        const vector<string>& outputs,
        const OpFactory& factory) {
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
        OpDef def;
        def.name = name;
        def.inputs = inputs;
        def.outputs = outputs;
        def.factory = factory;
        auto pr = m_ops.insert(make_pair(name, def));
        if (!pr.second)
            throw invalid_argument("op already defined: " + name);
        return *this;
    }

    Graph* Graph::Builder::build(const string& name) const {
        Graph* g = newGraph(name);
        for (auto& name : m_vars) {
            g->defVar(name);
        }
        for (auto& pr : m_ops) {
            g->addOp(pr.second.name,
                pr.second.inputs, pr.second.outputs,
                pr.second.factory());
        }
        return g;
    }
}
