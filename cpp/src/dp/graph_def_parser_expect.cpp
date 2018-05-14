#include <stdexcept>

#include "graph_def_parse.h"

namespace dp {
    using namespace std;

    expect::expect(token_iter it) : m_iter(it) {
    }

    expect& expect::skip_sp() {
        m_entries.push_back(make_pair(
            [] (token_iter it) -> bool {
                switch (it->type) {
                case tt_sp:
                case tt_comment:
                    return true;
                }
                return false;
            },
            [] (token_iter) {}
        ));
        return *this;
    }

    expect& expect::skip_allsp() {
        m_entries.push_back(make_pair(
            [] (token_iter it) -> bool {
                switch (it->type) {
                case tt_sp:
                case tt_comment:
                case tt_newline:
                    return true;
                }
                return false;
            },
            [] (token_iter) {}
        ));
        return *this;
    }

    expect& expect::on_newline(::std::function<void(token_iter)> fn) {
        m_entries.push_back(make_pair(
            [] (token_iter it) -> bool {
                return it->type == tt_newline;
            },
            fn
        ));
        return *this;
    }

    expect& expect::on_op(int ch, ::std::function<void(token_iter)> fn) {
        m_entries.push_back(make_pair(
            [ch] (token_iter it) -> bool {
                return it->type == tt_op && it->op == ch;
            },
            fn
        ));
        return *this;
    }

    expect& expect::on_sym(::std::function<void(token_iter)> fn) {
        m_entries.push_back(make_pair(
            [] (token_iter it) -> bool {
                return it->type == tt_literal && it->subtype != tt_sub_string;
            },
            fn
        ));
        return *this;
    }

    expect& expect::on_sym_of(const ::std::string& sym, ::std::function<void(token_iter)> fn) {
        m_entries.push_back(make_pair(
            [sym] (token_iter it) -> bool {
                return it->type == tt_literal && it->subtype != tt_sub_string && it->parsed.compare(sym) == 0;
            },
            fn
        ));
        return *this;
    }

    expect& expect::on_kw(keyword kw, ::std::function<void(token_iter)> fn) {
        m_entries.push_back(make_pair(
            [kw] (token_iter it) -> bool {
                return it->type == tt_literal && it->subtype == tt_sub_keyword && it->kw == kw;
            },
            fn
        ));
        return *this;
    }

    expect& expect::on_literal(::std::function<void(token_iter)> fn) {
        m_entries.push_back(make_pair(
            [] (token_iter it) -> bool {
                return it->type == tt_literal;
            },
            fn
        ));
        return *this;
    }

    expect& expect::otherwise(::std::function<void(token_iter)> fn) {
        m_otherwise = fn;
        return *this;
    }

    void expect::run() {
        for (auto& ent : m_entries) {
            if (ent.first && ent.first(m_iter)) {
                if (ent.second) ent.second(m_iter);
                return;
            }
        }
        if (m_otherwise) m_otherwise(m_iter);
        else {
            throw logic_error("uncaught situation");
        }
    }
}
