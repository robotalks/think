#include <cctype>
#include <cstring>
#include <stdexcept>

#include "graph_def_parse.h"

namespace dp {
    using namespace std;

    parser::parser()
    : m_state(s_expect_keyword) {
    }

    void parser::add(const token& t) {
        expect exp(m_ast.tokens.insert(m_ast.tokens.end(), t));
        switch (m_state) {
        case s_expect_keyword:
            exp.skip_allsp()
                .on_kw(kw_graph, [this] (token_iter it) {
                    commit_graph();
                    m_state = s_g_expect_name;
                })
                .on_kw(kw_arg, set_state(s_arg_expect_name))
                .on_kw(kw_op, set_state(s_op_expect_out_var_or_name))
                .on_kw(kw_in, [this] (token_iter it) {
                    begin_vars(var_in);
                    m_back = [this] (token_iter it) {
                        expect(it).skip_allsp()
                            .on_op('=', set_state(s_in_expect_name))
                            .otherwise(throws("expect '='"))
                            .run();
                    };
                })
                .on_sym(save_set_state(s_op_expect_out_var_or_name_next))
                .otherwise(throws("expect keyword"))
                .run();
            break;
        case s_g_expect_name:
            exp.skip_allsp()
                .on_sym([this] (token_iter it) {
                    m_ast.graphs.must_add(*it, graph_scheme{it->parsed, it});
                    m_state = s_g_expect_end;
                })
                .otherwise(throws("expect graph name"))
                .run();
            break;
        case s_g_expect_end:
            exp.skip_sp()
                .on_newline(set_state(s_expect_keyword))
                .otherwise(throws("expect newline"))
                .run();
            break;
        case s_arg_expect_name:
            exp.skip_allsp()
                .on_sym([this] (token_iter it) {
                    g().args.must_add(*it, arg_def{it, nullptr, m_ast.tokens.end()});
                    m_state = s_arg_expect_assign;
                })
                .otherwise(throws("expect arg name"))
                .run();
            break;
        case s_arg_expect_assign:
            exp.skip_sp()
                .on_newline(set_state(s_expect_keyword))
                .on_op('=', set_state(s_arg_expect_value))
                .on_op(',', set_state(s_arg_expect_name))
                .otherwise(throws("expect '=', ',' or newline"))
                .run();
            break;
        case s_arg_expect_value:
            exp.skip_allsp()
                .on_literal([this] (token_iter it) {
                    auto& r = g().args.back();
                    r.value = &*it;
                    r.value_iter = it;
                    m_state = s_arg_expect_end;
                })
                .otherwise(throws("expect default value"))
                .run();
        case s_arg_expect_end:
            exp.skip_sp()
                .on_newline(set_state(s_expect_keyword))
                .on_op(',', set_state(s_arg_expect_name))
                .otherwise(throws("expect ',' or newline"))
                .run();
            break;
        case s_op_expect_out_var_or_name:
            exp.skip_allsp()
                .on_sym(save_set_state(s_op_expect_out_var_or_name_next))
                .otherwise(throws("expect var name or op name"))
                .run();
            break;
        case s_op_expect_out_var_or_name_next:
            exp.skip_allsp()
                .on_op(',', [this] (token_iter it) {
                    begin_vars_restore(var_out);
                    m_back = [this] (token_iter it) {
                        expect(it).skip_allsp()
                            .on_op('=', set_state(s_op_expect_name))
                            .otherwise(throws("expect '='"))
                            .run();
                    };
                })
                .on_op('(', [this] (token_iter it) {
                    commit_op(m_save);
                    begin_vars(var_in);
                    m_back = expect_op_in_vars_end();
                })
                .on_op('=', [this] (token_iter it) {
                    begin_vars_restore(var_out);
                    m_state = s_op_expect_name;
                })
                .otherwise(throws("expect ',', '(' or '='"))
                .run();
            break;
        case s_op_expect_name:
            exp.skip_allsp()
                .on_sym([this] (token_iter it) {
                    commit_op(it);
                    begin_vars(var_in);
                    m_state = s_vars_expect_begin;
                    m_back = expect_op_in_vars_end();
                })
                .otherwise(throws("expect op name"))
                .run();
            break;
        case s_op_expect_use:
            exp.skip_allsp()
                .on_op(':', set_state(s_op_expect_type))
                .on_sym_of("use", set_state(s_op_expect_type))
                .otherwise(throws("expect ':' or 'use'"))
                .run();
            break;
        case s_op_expect_type:
            exp.skip_allsp()
                .on_sym([this] (token_iter it) {
                    g().ops.back().factory = it;
                    m_state = s_params_expect_begin;
                    m_back = [this] (token_iter it) {
                        auto& d = g().ops.back();
                        d.params.splice(d.params.end(), m_params);
                        m_state = s_op_expect_end;
                    };
                })
                .otherwise(throws("expect op type"))
                .run();
            break;
        case s_op_expect_end:
            exp.skip_sp()
                .on_newline(set_state(s_expect_keyword))
                .on_op(',', set_state(s_op_expect_out_var_or_name))
                .otherwise(throws("expect ',' or newline"))
                .run();
            break;
        case s_in_expect_name:
            exp.skip_allsp()
                .on_sym([this] (token_iter it) {
                    auto i = g().ingresses.must_add(*it, ingress_def{it});
                    i->input_vars.splice(i->input_vars.end(), m_var_refs);
                    m_state = s_in_expect_use;
                })
                .otherwise(throws("expect ingress name"))
                .run();
            break;
        case s_in_expect_use:
            exp.skip_allsp()
                .on_op(':', set_state(s_in_expect_type))
                .on_sym_of("use", set_state(s_in_expect_type))
                .otherwise(throws("expect ':' or 'use'"))
                .run();
            break;
        case s_in_expect_type:
            exp.skip_allsp()
                .on_sym([this] (token_iter it) {
                    g().ingresses.back().factory = it;
                    m_state = s_params_expect_begin;
                    m_back = [this] (token_iter it) {
                        auto& d = g().ingresses.back();
                        d.params.splice(d.params.end(), m_params);
                        m_state = s_in_expect_end;
                    };
                })
                .otherwise(throws("expect ingress type"))
                .run();
            break;
        case s_in_expect_end:
            exp.skip_sp()
                .on_newline(set_state(s_expect_keyword))
                .on_op(',', set_state(s_in_expect_name))
                .otherwise(throws("expect ',' or newline"))
                .run();
            break;
        case s_vars_expect_begin:
            exp.skip_allsp()
                .on_op('(', set_state(s_vars_expect_name))
                .otherwise(throws("expect '('"))
                .run();
            break;
        case s_vars_expect_name:
            exp.skip_allsp()
                .on_sym([this] (token_iter it) {
                    m_var_refs.push_back(var_ref{it, m_var_usage});
                    m_state = s_vars_expect_end;
                })
                .otherwise(m_back)
                .run();
            break;
        case s_vars_expect_end:
            exp.skip_allsp()
                .on_op(',', set_state(s_vars_expect_name))
                .otherwise(m_back)
                .run();
            break;
        case s_params_expect_begin:
            exp.skip_allsp()
                .on_op('{', set_state(s_params_expect_key))
                .otherwise(throws("expect '{'"))
                .run();
            break;
        case s_params_expect_key:
            exp.skip_allsp()
                .on_op('}', m_back)
                .on_literal([this] (token_iter it) {
                    m_params.push_back(param_entry{it});
                    m_state = s_params_expect_colon;
                })
                .otherwise(throws("expect parameter key"))
                .run();
            break;
        case s_params_expect_colon:
            exp.skip_allsp()
                .on_op(':', set_state(s_params_expect_value))
                .otherwise(throws("expect ':'"))
                .run();
            break;
        case s_params_expect_value:
            exp.skip_allsp()
                .on_literal([this] (token_iter it) {
                    m_params.back().val = it;
                    m_state = s_params_expect_end;
                })
                .otherwise(throws("expect value"))
                .run();
            break;
        case s_params_expect_end:
            exp.skip_allsp()
                .on_op(',', set_state(s_params_expect_key))
                .on_op('}', m_back)
                .otherwise(throws("expect ',' or '}'"))
                .run();
            break;
        }
    }

    void parser::add(const token_list& tokens) {
        for (auto& t : tokens) {
            add(t);
        }
    }

    ast parser::end() {
        commit_graph();
        return move(m_ast);
    }

    graph_scheme& parser::g() {
        if (m_ast.graphs.empty()) m_ast.graphs.add("", graph_scheme{"", m_ast.tokens.end()});
        return m_ast.graphs.back();
    }

    ::std::function<void(token_iter)> parser::set_state(state s) {
        return [this, s] (token_iter) { m_state = s; };
    }

    ::std::function<void(token_iter)> parser::save_set_state(state s) {
        return [this, s] (token_iter it) { m_save = it; m_state = s; };
    }

    ::std::function<void(token_iter)> parser::throws(::std::string msg) {
        return [&msg] (token_iter it) { throw graph_def::parse_error(it->loc, msg); };
    }

    void parser::begin_vars(var_usage usage) {
        m_var_usage = usage;
        m_var_refs.clear();
        m_state = s_vars_expect_name;
    }

    void parser::begin_vars_restore(var_usage usage) {
        m_var_usage = usage;
        m_var_refs.clear();
        m_var_refs.push_back(var_ref{m_save, usage});
        m_state = s_vars_expect_name;
    }

    void parser::commit_graph() {
        graph_def::location loc;

        switch (m_state) {
        case s_expect_keyword:
        case s_g_expect_end:
        case s_arg_expect_assign:
        case s_arg_expect_end:
        case s_op_expect_end:
        case s_in_expect_end:
            break;
        default:
            if (!m_ast.tokens.empty()) {
                auto& t = m_ast.tokens.back();
                loc = t.loc;
                loc.off += t.text.length();
                loc.col += t.text.length();
            }
            throw graph_def::parse_error(loc, "unexpected end");
        }
        m_state = s_expect_keyword;
    }

    void parser::commit_op(token_iter it) {
        auto i = g().ops.must_add(*it, op_def{it});
        i->o_vars.splice(i->o_vars.end(), m_var_refs);
    }

    function<void(token_iter)> parser::expect_op_in_vars_end() {
        return [this] (token_iter it) {
            expect(it).skip_allsp()
                .on_op(')', [this] (token_iter it) {
                    auto& d = g().ops.back();
                    d.i_vars.splice(d.i_vars.end(), m_var_refs);
                    m_state = s_op_expect_use;
                })
                .otherwise(throws("expect ',' or ')'"))
                .run();
        };
    }
}
