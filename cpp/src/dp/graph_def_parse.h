#ifndef __DP_GRAPH_DEF_PARSE_H
#define __DP_GRAPH_DEF_PARSE_H

#include <string>
#include <list>
#include <istream>
#include "dp/graph_def.h"

namespace dp {
    enum token_type {
        tt_sp = 0,
        tt_newline,
        tt_comment,
        tt_literal,
        tt_op,
    };

    enum token_subtype {
        tt_sub_none = 0,
        tt_sub_string,
        tt_sub_keyword,
    };

    enum keyword {
        kw_none = 0,
        kw_graph,
        kw_arg,
        kw_op,
        kw_in,
    };

    struct token {
        graph_def::location loc;
        token_type type;
        token_subtype subtype;
        ::std::string text;
        ::std::string parsed;
        union {
            keyword kw;
            int op;
        };
    };

    using token_list = ::std::list<token>;
    using token_iter = token_list::iterator;

    class tokenizer {
    public:
        tokenizer();

        token_list add(int ch);
        token_list end();

        token_list add(::std::istream&);
        token_list add(const ::std::string&, bool end = false);

    private:
        token m_token;
        graph_def::location m_cur;

        enum state {
            is_none = 0,
            is_comment,
            is_literal,
            is_escapable_str,
            is_escape,
            is_non_escapable_str,
        };

        state m_state;

        void emit(token_list& tokens, token_type tt, token_subtype ts = tt_sub_none);
        void move_in(int ch, int parsed = 0);
    };

    template<typename T>
    class indexed_list {
    public:
        indexed_list() { }
        indexed_list(const indexed_list& l)
        : m_items(l.m_items), m_names(l.m_names) {
        }
        indexed_list(indexed_list&& l)
        : m_items(::std::move(l.m_items)), m_names(::std::move(l.m_names)) {
        }

        indexed_list& operator = (const indexed_list& l) {
            m_items = l.m_items;
            m_names = l.m_names;
            return *this;
        }

        indexed_list& operator = (indexed_list&& l) {
            m_items = ::std::move(l.m_items);
            m_names = ::std::move(l.m_names);
            return *this;
        }

        typename ::std::list<T>::iterator add(const ::std::string& name, const T& item) {
            auto pr = m_names.insert(::std::make_pair(name, m_items.end()));
            if (pr.second) {
                pr.first->second = m_items.insert(m_items.end(), item);
            }
            return pr.first->second;
        }

        typename ::std::list<T>::iterator must_add(const token& t, const T& item) {
            auto pr = m_names.insert(::std::make_pair(t.parsed, m_items.end()));
            if (pr.second) {
                pr.first->second = m_items.insert(m_items.end(), item);
                return pr.first->second;
            }
            throw graph_def::parse_error(t.loc, "already defined " + t.parsed);
        }

        typename ::std::list<T>::iterator find(const ::std::string& name) {
            auto it = m_names.find(name);
            return it == m_names.end() ? end() : it->second;
        }

        typename ::std::list<T>::const_iterator find(const ::std::string& name) const {
            auto it = m_names.find(name);
            return it == m_names.end() ? end() : it->second;
        }

        typename ::std::list<T>::iterator must_find(const token& t) {
            auto it = m_names.find(t.parsed);
            if (it == m_names.end()) {
                throw graph_def::parse_error(t.loc, "undefined: " + t.parsed);
            }
            return it->second;
        }

        typename ::std::list<T>::iterator begin() { return m_items.begin(); }
        typename ::std::list<T>::iterator end() { return m_items.end(); }

        typename ::std::list<T>::const_iterator begin() const { return m_items.begin(); }
        typename ::std::list<T>::const_iterator end() const { return m_items.end(); }

        T& front() { return m_items.front(); }
        T& back() { return m_items.back(); }

        const T& front() const { return m_items.front(); }
        const T& back() const { return m_items.back(); }

        bool empty() const { return m_items.empty(); }
        size_t size() const { return m_items.size(); }

    private:
        typename ::std::list<T> m_items;
        typename ::std::unordered_map<::std::string, typename ::std::list<T>::iterator> m_names;
    };

    struct param_entry {
        token_iter key;
        token_iter val;
    };

    struct arg_def {
        token_iter name;
        token *value;
        token_iter value_iter;
    };

    enum var_usage {
        var_unknown = 0,
        var_in,
        var_out,
    };

    struct var_ref {
        token_iter name;
        var_usage as;
    };

    struct op_def {
        token_iter name;
        ::std::list<var_ref> i_vars;
        ::std::list<var_ref> o_vars;
        token_iter factory;
        ::std::list<param_entry> params;
    };

    struct ingress_def {
        token_iter name;
        ::std::list<var_ref> input_vars;
        token_iter factory;
        ::std::list<param_entry> params;
    };

    struct graph_scheme {
        ::std::string name;
        token_iter name_iter;
        indexed_list<arg_def> args;
        indexed_list<op_def> ops;
        indexed_list<ingress_def> ingresses;
    };

    struct ast {
        token_list tokens;
        indexed_list<graph_scheme> graphs;

        ast() { }

        ast(const ast& a)
        : tokens(a.tokens), graphs(a.graphs) {
        }

        ast(ast&& a)
        : tokens(::std::move(a.tokens)), graphs(::std::move(a.graphs)) {
        }

        ast& operator = (const ast& a) {
            tokens = a.tokens;
            graphs = a.graphs;
            return *this;
        }

        ast& operator = (ast&& a) {
            tokens = ::std::move(a.tokens);
            graphs = ::std::move(a.graphs);
            return *this;
        }
    };

    class parser {
    public:
        parser();

        enum state {
            s_expect_keyword = 0,
            s_g_expect_name,
            s_g_expect_end,
            s_arg_expect_name,
            s_arg_expect_assign,
            s_arg_expect_value,
            s_arg_expect_end,
            s_op_expect_out_var_or_name,
            s_op_expect_out_var_or_name_next,
            s_op_expect_name,
            s_op_expect_use,
            s_op_expect_type,
            s_op_expect_end,
            s_in_expect_name,
            s_in_expect_use,
            s_in_expect_type,
            s_in_expect_end,
            s_vars_expect_begin,
            s_vars_expect_name,
            s_vars_expect_end,
            s_params_expect_begin,
            s_params_expect_key,
            s_params_expect_colon,
            s_params_expect_value,
            s_params_expect_end,
        };

        void add(const token&);
        void add(const token_list&);
        ast end();

    private:
        ast m_ast;

        graph_scheme& g();

        token_iter m_save;
        ::std::list<var_ref> m_var_refs;
        var_usage m_var_usage;
        ::std::list<param_entry> m_params;

        state m_state;
        ::std::function<void(token_iter)> m_back;

        ::std::function<void(token_iter)> set_state(state s);
        ::std::function<void(token_iter)> save_set_state(state s);
        ::std::function<void(token_iter)> throws(::std::string msg);

        void begin_vars(var_usage);
        void begin_vars_restore(var_usage);

        void commit_graph();

        void commit_op(token_iter);
        ::std::function<void(token_iter)> expect_op_in_vars_end();
    };

    class expect {
    public:
        expect(token_iter);

        expect& skip_sp();
        expect& skip_allsp();
        expect& on_newline(::std::function<void(token_iter)> fn);
        expect& on_op(int ch, ::std::function<void(token_iter)> fn);
        expect& on_sym(::std::function<void(token_iter)> fn);
        expect& on_kw(keyword kw, ::std::function<void(token_iter)> fn);
        expect& on_sym_of(const ::std::string& sym, ::std::function<void(token_iter)> fn);
        expect& on_literal(::std::function<void(token_iter)> fn);
        expect& otherwise(::std::function<void(token_iter)> fn);

        void run();
    private:
        token_iter m_iter;
        ::std::list<::std::pair<::std::function<bool(token_iter)>, ::std::function<void(token_iter)>>> m_entries;
        ::std::function<void(token_iter)> m_otherwise;
    };

    // cross-references

    struct xref_arg {
        ::std::list<arg_def>::iterator s_iter;
        int ref_count;

        const ::std::string& name() const { return s_iter->name->parsed; }
        bool has_value() const { return s_iter->value != nullptr; }
        ::std::string value() const { return has_value() ? s_iter->value->parsed : ::std::string(); }

        xref_arg(::std::list<arg_def>::iterator it)
        : s_iter(it), ref_count(0) {
        }
    };

    struct xref_var {
        token_iter s_iter;
        var_usage usage;
        int ref_count;
        int ingress_count;

        const ::std::string& name() const { return s_iter->parsed; }

        xref_var(token_iter it, var_usage u)
        : s_iter(it), usage(u), ref_count(0) {
        }
    };

    struct xref_ingress {
        ::std::list<ingress_def>::iterator s_iter;
        ::std::list<::std::list<xref_var>::iterator> inputs;
        graph_def::ingress_factory* factory;

        const ::std::string& name() const { return s_iter->name->parsed; }

        xref_ingress(::std::list<ingress_def>::iterator it)
        : s_iter(it) {
        }
    };

    struct xref_op {
        ::std::list<op_def>::iterator s_iter;
        ::std::list<::std::list<xref_var>::iterator> i_vars;
        ::std::list<::std::list<xref_var>::iterator> o_vars;
        graph_def::op_factory* factory;

        const ::std::string& name() const { return s_iter->name->parsed; }

        xref_op(::std::list<op_def>::iterator it)
        : s_iter(it) {
        }
    };

    struct xref_graph {
        ::std::list<graph_scheme>::iterator s_iter;
        indexed_list<xref_arg> args;
        indexed_list<xref_var> vars;
        indexed_list<xref_ingress> ingresses;
        indexed_list<xref_op> ops;

        const ::std::string& name() const { return s_iter->name; }

        xref_graph(::std::list<graph_scheme>::iterator it)
        : s_iter(it) {
        }
    };

    class graph_def::xref {
    public:
        xref(ast&&, graph_def::op_registry*, graph_def::ingress_registry*);

        void build_refs();

        void build_graph(const xref_graph&, graph&, const graph_def::params&);

        const indexed_list<xref_graph>& graphs() const { return m_graphs; }

    private:
        ast m_ast;
        graph_def::op_registry *m_ops;
        graph_def::ingress_registry *m_ins;

        indexed_list<xref_graph> m_graphs;

        void build_graph_refs(xref_graph&);
    };
}

#endif
