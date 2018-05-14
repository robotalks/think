#include <cstring>
#include <stdexcept>
#include <fstream>
#include <unordered_set>

#include "graph_def_parse.h"

namespace dp {
    using namespace std;

    graph_def::op_registry::op_registry() {
    }

    graph::op_func graph_def::op_registry::create_op(
        const string& name, const string& type, const graph_def::params& params) {
        auto f = get_factory(type);
        if (f == nullptr) {
            throw invalid_argument("invalid op " + type);
        }
        return f->create_op(name, type, params);
    }

    void graph_def::op_registry::add_factory(const ::std::string& name, op_factory* f) {
        if (!m_factories.insert(make_pair(name, f)).second) {
            throw logic_error("factory already registered: " + name);
        }
    }

    graph_def::op_factory* graph_def::op_registry::get_factory(const ::std::string& name) const {
        auto it = m_factories.find(name);
        return it == m_factories.end() ? nullptr : it->second;
    }

    graph_def::ingress_registry::ingress_registry() {
    }

    ingress* graph_def::ingress_registry::create_ingress(
        const string& name, const string& type, const graph_def::params& params) {
        auto f = get_factory(type);
        if (f == nullptr) {
            throw invalid_argument("invalid ingress " + type);
        }
        return f->create_ingress(name, type, params);
    }

    void graph_def::ingress_registry::add_factory(const ::std::string& name, ingress_factory* f) {
        if (!m_factories.insert(make_pair(name, f)).second) {
            throw logic_error("factory already registered: " + name);
        }
    }

    graph_def::ingress_factory* graph_def::ingress_registry::get_factory(const ::std::string& name) const {
        auto it = m_factories.find(name);
        return it == m_factories.end() ? nullptr : it->second;
    }

    string graph_def::location::prefix() const {
        char str[256];
        sprintf(str, "%lu:L%luC%lu:", off, line+1, col+1);
        return str;
    }

    static graph_def::params make_params(const list<param_entry>& entries,
        const indexed_list<xref_arg>& arg_refs, const graph_def::params& args) {
        graph_def::params p;
        for (auto& ent : entries) {
            p[ent.key->parsed] = ent.val->parsed;
        }
        return move(p);
    }

    graph_def::graph_def() {
    }

    graph_def::graph_def(op_registry *ops, ingress_registry *ingresses) {
        use_ops(ops);
        use_ingresses(ingresses);
    }

    graph_def& graph_def::use_ops(op_registry *ops) {
        m_ops = ops;
        return *this;
    }

    graph_def& graph_def::use_ingresses(ingress_registry *ins) {
        m_ins = ins;
        return *this;
    }

    graph_def& graph_def::parse(::std::istream& is) {
        parser p;
        p.add(tokenizer().add(is));
        auto ast = p.end();
        build_xref(new xref(move(ast), m_ops, m_ins));
        return *this;
    }

    graph_def& graph_def::parse(const ::std::string& s) {
        parser p;
        p.add(tokenizer().add(s, true));
        auto ast = p.end();
        build_xref(new xref(move(ast), m_ops, m_ins));
        return *this;
    }

    graph_def& graph_def::parse_file(const ::std::string& fn) {
        ifstream is;
        is.exceptions(ifstream::failbit | ifstream::badbit);
        is.open(fn.c_str());
        return parse(is);
    }

    graph_def& graph_def::select_graph(const ::std::string& name) {
        for (auto& g : m_refs->graphs()) {
            if (g.name().compare(name) == 0) {
                m_selected_graph = name;
                return *this;
            }
        }
        throw invalid_argument("graph not found: " + name);
    }

    exec_env& graph_def::build_env(exec_env& env, const graph_def::params& args) const {
        env.graphs.push_back(move(unique_ptr<graph>(build_graph(args))));
        env.ingresses = move(build_ingresses(args));
        env.build();
        return env;
    }

    vector<string> graph_def::graph_names() const {
        vector<string> names;
        for (auto& g : m_refs->graphs()) {
            names.push_back(g.name());
        }
        return move(names);
    }

    graph* graph_def::build_graph(const graph_def::params& args) const {
        return build_graph(m_selected_graph, args);
    }

    graph* graph_def::build_graph(const string& name, const graph_def::params& args) const {
        auto xg = m_refs->graphs().find(name);
        if (xg == m_refs->graphs().end()) {
            throw invalid_argument("graph not found: " + name);
        }
        graph* g = new graph();
        try {
            m_refs->build_graph(*xg, *g, args);
            return g;
        } catch (const exception& e) {
            delete g;
            throw e;
        }
    }

    list<unique_ptr<ingress>> graph_def::build_ingresses(const graph_def::params& args) const {
        return move(build_ingresses_in_graph(m_selected_graph, args));
    }

    list<unique_ptr<ingress>> graph_def::build_ingresses_in_graph(
        const string& graph_name, const graph_def::params& args) const {
        auto xg = m_refs->graphs().find(graph_name);
        if (xg == m_refs->graphs().end()) {
            throw invalid_argument("graph not found: " + graph_name);
        }
        list<unique_ptr<ingress>> ins;
        for (auto it = xg->ingresses.begin(); it != xg->ingresses.end(); it ++) {
            unique_ptr<ingress> inp(it->factory->create_ingress(
                it->name(),
                it->s_iter->factory->parsed,
                make_params(it->s_iter->params, xg->args, args)));
            ins.push_back(move(inp));
        }
        return move(ins);
    }

    void graph_def::build_xref(graph_def::xref *xr) {
        unique_ptr<xref> r(xr);
        r->build_refs();
        if (!r->graphs().empty()) {
            m_selected_graph = r->graphs().front().name();
        } else {
            m_selected_graph.clear();
        }
        m_refs = move(r);
    }

    graph_def::xref::xref(ast&& a,
        graph_def::op_registry *ops, graph_def::ingress_registry* ins)
    : m_ast(move(a)), m_ops(ops), m_ins(ins) {
    }

    void graph_def::xref::build_refs() {
        auto g = m_ast.graphs.begin();
        for (; g != m_ast.graphs.end(); g ++) {
            auto xg = m_graphs.add(g->name, xref_graph(g));
            build_graph_refs(*xg);
        }
    }

    void graph_def::xref::build_graph_refs(xref_graph& xg) {
        for (auto it = xg.s_iter->args.begin(); it != xg.s_iter->args.end(); it ++) {
            xg.args.must_add(*it->name, xref_arg(it));
        }

        for (auto it = xg.s_iter->ingresses.begin(); it != xg.s_iter->ingresses.end(); it ++) {
            xref_ingress ing(it);
            ing.factory = m_ins->get_factory(it->factory->parsed);
            if (!ing.factory)
                throw graph_def::parse_error(it->factory->loc,
                    "ingress type not found: " + it->factory->parsed);
            unordered_set<string> names;
            for (auto& r : it->input_vars) {
                if (!names.insert(r.name->parsed).second) {
                    throw graph_def::parse_error(r.name->loc,
                        "var " + r.name->parsed + " duplicated");
                }
                auto ins = xg.vars.add(r.name->parsed,
                    xref_var(r.name, var_in));
                if (ins->usage != var_in) {
                    throw graph_def::parse_error(r.name->loc,
                        "var " + r.name->parsed + " used as output");
                }
                ins->ingress_count ++;
                ing.inputs.push_back(ins);
            }
            xg.ingresses.must_add(*it->name, ing);
        }

        for (auto it = xg.s_iter->ops.begin(); it != xg.s_iter->ops.end(); it ++) {
            xref_op op(it);
            op.factory = m_ops->get_factory(it->factory->parsed);
            if (!op.factory)
                throw graph_def::parse_error(it->factory->loc,
                    "op type not found: " + it->factory->parsed);
            for (auto& r : it->i_vars) {
                auto v_it = xg.vars.must_find(*r.name);
                op.i_vars.push_back(v_it);
                v_it->ref_count ++;
            }
            unordered_set<string> names;
            for (auto& r : it->o_vars) {
                if (!names.insert(r.name->parsed).second) {
                    throw graph_def::parse_error(r.name->loc,
                        "var " + r.name->parsed + " duplicated");
                }
                auto v_it = xg.vars.must_add(*r.name, xref_var(r.name, var_out));
                op.o_vars.push_back(v_it);
            }
            xg.ops.must_add(*it->name, op);
        }

        for (auto it = xg.vars.begin(); it != xg.vars.end(); it ++) {
            if (it->usage == var_in && it->ref_count == 0) {
                throw graph_def::parse_error(it->s_iter->loc,
                    "var " + it->name() + " not used");
            }
        }
    }

    void graph_def::xref::build_graph(const xref_graph& xg, graph& g,
        const graph_def::params& args) {
        for (auto it = xg.vars.begin(); it != xg.vars.end(); it ++) {
            g.def_var(it->name());
        }
        for (auto it = xg.ops.begin(); it != xg.ops.end(); it ++) {
            vector<string> in, out;
            for (auto v_it : it->i_vars) {
                in.push_back(v_it->name());
            }
            for (auto v_it : it->o_vars) {
                out.push_back(v_it->name());
            }
            g.add_op(it->name(), in, out,
                it->factory->create_op(
                    it->name(),
                    it->s_iter->factory->parsed,
                    make_params(it->s_iter->params, xg.args, args)));
        }
    }
}
