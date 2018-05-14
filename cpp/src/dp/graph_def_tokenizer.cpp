#include <cctype>
#include <cstring>
#include <stdexcept>

#include "graph_def_parse.h"

namespace dp {
    using namespace std;

    static bool is_ch_operator(int ch) {
        return strchr(",=(){}:", ch) != nullptr;
    }

    static bool is_ch_literal_start(int ch) {
        return isalnum(ch) || strchr("_$.@", ch) != nullptr;
    }

    static bool is_ch_literal(int ch) {
        return isalnum(ch) || strchr("_$.@", ch) != nullptr;
    }

    static keyword is_keyword(const string& text) {
        if (text.compare("graph") == 0) return kw_graph;
        if (text.compare("arg") == 0) return kw_arg;
        if (text.compare("op") == 0) return kw_op;
        if (text.compare("in") == 0) return kw_in;
        return kw_none;
    }

    tokenizer::tokenizer()
    : m_state(is_none) {
        m_token.loc = m_cur;
    }

    token_list tokenizer::add(int ch) {
        token_list tokens;
        switch (m_state) {
        case is_none:
            if (ch == '#') {
                emit(tokens, tt_sp);
                move_in(ch);
                m_state = is_comment;
            } else if (is_ch_operator(ch)) {
                emit(tokens, tt_sp);
                move_in(ch, ch);
                emit(tokens, tt_op);
            } else if (ch == '\n') {
                emit(tokens, tt_sp);
                move_in(ch, ch);
                emit(tokens, tt_newline);
            } else if (ch == '"') {
                emit(tokens, tt_sp);
                move_in(ch);
                m_state = is_escapable_str;
            } else if (ch == '\'') {
                emit(tokens, tt_sp);
                move_in(ch);
                m_state = is_non_escapable_str;
            } else if (is_ch_literal_start(ch)) {
                emit(tokens, tt_sp);
                move_in(ch, ch);
                m_state = is_literal;
            } else if (isspace(ch)) {
                move_in(ch);
            } else {
                throw graph_def::parse_error(m_cur, "illegal character");
            }
            break;
        case is_comment:
            if (ch == '\n') {
                emit(tokens, tt_comment);
                move_in(ch, ch);
                emit(tokens, tt_newline);
                m_state = is_none;
            } else {
                move_in(ch, ch);
            }
            break;
        case is_literal:
            if (ch == '#') {
                emit(tokens, tt_literal);
                move_in(ch);
                m_state = is_comment;
            } else if (is_ch_operator(ch)) {
                emit(tokens, tt_literal);
                move_in(ch, ch);
                emit(tokens, tt_op);
                m_state = is_none;
            } else if (ch == '\n') {
                emit(tokens, tt_literal);
                move_in(ch, ch);
                emit(tokens, tt_newline);
                m_state = is_none;
            } else if (ch == '"') {
                emit(tokens, tt_literal);
                move_in(ch);
                m_state = is_escapable_str;
            } else if (ch == '\'') {
                emit(tokens, tt_literal);
                move_in(ch);
                m_state = is_non_escapable_str;
            } else if (isspace(ch)) {
                emit(tokens, tt_literal);
                move_in(ch);
                m_state = is_none;
            } else if (is_ch_literal(ch)) {
                move_in(ch, ch);
            } else {
                throw graph_def::parse_error(m_cur, "illegal character");
            }
            break;
        case is_escapable_str:
            if (ch == '"') {
                move_in(ch);
                emit(tokens, tt_literal, tt_sub_string);
                m_state = is_none;
            } else if (ch == '\\') {
                move_in(ch);
                m_state = is_escape;
            } else {
                move_in(ch, ch);
            }
            break;
        case is_escape:
            switch (ch) {
            case 'r': move_in(ch, '\r'); break;
            case 'n': move_in(ch, '\n'); break;
            case 't': move_in(ch, '\t'); break;
            case 'f': move_in(ch, '\f'); break;
            case 'a': move_in(ch, '\a'); break;
            case 'b': move_in(ch, '\b'); break;
            case 'v': move_in(ch, '\v'); break;
            default: move_in(ch, ch); break;
            }
            m_state = is_escapable_str;
            break;
        case is_non_escapable_str:
            if (ch == '\'') {
                move_in(ch);
                emit(tokens, tt_literal, tt_sub_string);
                m_state = is_none;
            } else {
                move_in(ch, ch);
            }
            break;
        }
        return move(tokens);
    }

    token_list tokenizer::end() {
        token_list tokens;
        switch (m_state) {
        case is_none:
            emit(tokens, tt_sp);
            break;
        case is_comment:
            emit(tokens, tt_comment);
            break;
        case is_literal:
            emit(tokens, tt_literal);
            break;
        case is_escapable_str:
            throw graph_def::parse_error(m_cur, "unexpected end of string");
        case is_escape:
            throw graph_def::parse_error(m_cur, "unexpected end of escape character");
        case is_non_escapable_str:
            throw graph_def::parse_error(m_cur, "unexpected end of string");
        }
        return move(tokens);
    }

    token_list tokenizer::add(istream& is) {
        token_list tokens;
        while (!is.eof()) {
            tokens.splice(tokens.end(), add(is.get()));
        }
        tokens.splice(tokens.end(), end());
        return move(tokens);
    }

    token_list tokenizer::add(const string& str, bool end) {
        token_list tokens;
        for (size_t i = 0; i < str.length(); i ++) {
            tokens.splice(tokens.end(), add(str[i]));
        }
        if (end) {
            tokens.splice(tokens.end(), this->end());
        }
        return move(tokens);
    }

    void tokenizer::emit(token_list& tokens, token_type tt, token_subtype ts) {
        if (m_token.text.empty()) return;
        m_token.type = tt;
        if (tt == tt_literal && (ts == tt_sub_none || ts == tt_sub_keyword)) {
            m_token.kw = is_keyword(m_token.text);
            if (m_token.kw != kw_none) {
                ts = tt_sub_keyword;
            }
        }
        if (tt == tt_op) {
            m_token.op = m_token.text[0];
        }
        m_token.subtype = ts;
        tokens.push_back(m_token);
        m_token.loc = m_cur;
        m_token.text.clear();
        m_token.parsed.clear();
    }

    void tokenizer::move_in(int ch, int parsed) {
        m_token.text += (char)ch;
        if (parsed != 0) {
            m_token.parsed += (char)parsed;
        }
        m_cur.off ++;
        if (ch != '\n') {
            m_cur.col ++;
        } else {
            m_cur.line ++;
            m_cur.col = 0;
        }
    }
}
