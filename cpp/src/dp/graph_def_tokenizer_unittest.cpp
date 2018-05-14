#include <vector>
#include "gtest/gtest.h"

#include "graph_def_parse.h"

namespace dp {
    using namespace std;

    vector<token> tokenize_str(const string& str) {
        auto tokens = tokenizer().add(str, true);
        vector<token> v;
        for (auto& t : tokens) v.push_back(t);
        return move(v);
    }

    TEST(TokenizerTest, Empty) {
        auto tokens = tokenize_str("");
        EXPECT_EQ(0, tokens.size());
    }

    TEST(TokenizerTest, Comment) {
        auto tokens = tokenize_str("# comment");
        ASSERT_EQ(1, tokens.size());
        EXPECT_EQ(tt_comment, tokens[0].type);
        EXPECT_STREQ("# comment", tokens[0].text.c_str());
        EXPECT_STREQ(" comment", tokens[0].parsed.c_str());

        tokens = tokenize_str(" \t# comment");
        ASSERT_EQ(2, tokens.size());
        EXPECT_EQ(tt_sp, tokens[0].type);
        EXPECT_STREQ(" \t", tokens[0].text.c_str());
        EXPECT_EQ(tt_comment, tokens[1].type);
    }

    TEST(TokenizerTest, Operator) {
        const char *str = ",=(){}:_.";
        auto tokens = tokenize_str(str);
        ASSERT_EQ(8, tokens.size());
        for (int i = 0; i < 7; i ++) {
            EXPECT_EQ(tt_op, tokens[i].type);
            EXPECT_EQ(str[i], tokens[i].op);
        }
        EXPECT_EQ(tt_literal, tokens[7].type);

        tokens = tokenize_str("abc,def");
        ASSERT_EQ(3, tokens.size());
        EXPECT_EQ(tt_literal, tokens[0].type);
        EXPECT_EQ(tt_op, tokens[1].type);
        EXPECT_EQ(tt_literal, tokens[2].type);
    }

    TEST(TokenizerTest, Newline) {
        auto tokens = tokenize_str("\na\n");
        ASSERT_EQ(3, tokens.size());
        EXPECT_EQ(tt_newline, tokens[0].type);
        EXPECT_EQ(tt_literal, tokens[1].type);
        EXPECT_EQ(tt_newline, tokens[2].type);

        tokens = tokenize_str("# comment\r\n\r ,\na");
        ASSERT_EQ(6, tokens.size());
        EXPECT_EQ(tt_comment, tokens[0].type);
        EXPECT_STREQ("# comment\r", tokens[0].text.c_str());
        EXPECT_EQ(tt_newline, tokens[1].type);
        EXPECT_EQ(tt_sp, tokens[2].type);
        EXPECT_STREQ("\r ", tokens[2].text.c_str());
        EXPECT_EQ(tt_op, tokens[3].type);
        EXPECT_EQ(tt_newline, tokens[4].type);
        EXPECT_EQ(tt_literal, tokens[5].type);
    }

    TEST(TokenizerTest, EscapableStr) {
        auto tokens = tokenize_str(R"(abc"def\a\b\f\n\r\t\vg"123)");
        ASSERT_EQ(3, tokens.size());
        EXPECT_EQ(tt_literal, tokens[0].type);
        EXPECT_EQ(tt_sub_none, tokens[0].subtype);
        EXPECT_STREQ("abc", tokens[0].text.c_str());
        EXPECT_EQ(tt_literal, tokens[1].type);
        EXPECT_EQ(tt_sub_string, tokens[1].subtype);
        EXPECT_STREQ(R"("def\a\b\f\n\r\t\vg")", tokens[1].text.c_str());
        EXPECT_STREQ("def\a\b\f\n\r\t\vg", tokens[1].parsed.c_str());
        EXPECT_EQ(tt_literal, tokens[2].type);
        EXPECT_EQ(tt_sub_none, tokens[2].subtype);
        EXPECT_STREQ("123", tokens[2].text.c_str());
        EXPECT_STREQ("123", tokens[2].parsed.c_str());
    }

    TEST(TokenizerTest, NonEscapableStr) {
        auto tokens = tokenize_str(R"(abc'def\ng'123)");
        ASSERT_EQ(3, tokens.size());
        EXPECT_EQ(tt_literal, tokens[0].type);
        EXPECT_EQ(tt_sub_none, tokens[0].subtype);
        EXPECT_STREQ("abc", tokens[0].text.c_str());
        EXPECT_EQ(tt_literal, tokens[1].type);
        EXPECT_EQ(tt_sub_string, tokens[1].subtype);
        EXPECT_STREQ("'def\\ng'", tokens[1].text.c_str());
        EXPECT_STREQ("def\\ng", tokens[1].parsed.c_str());
        EXPECT_EQ(tt_literal, tokens[2].type);
        EXPECT_EQ(tt_sub_none, tokens[2].subtype);
        EXPECT_STREQ("123", tokens[2].text.c_str());
        EXPECT_STREQ("123", tokens[2].parsed.c_str());
    }

    TEST(TokenizerTest, Keyword) {
        auto tokens = tokenize_str("arg op in graph none");
        ASSERT_EQ(9, tokens.size());
        EXPECT_EQ(tt_literal, tokens[0].type);
        EXPECT_EQ(tt_sub_keyword, tokens[0].subtype);
        EXPECT_EQ(kw_arg, tokens[0].kw);
        EXPECT_EQ(tt_literal, tokens[2].type);
        EXPECT_EQ(tt_sub_keyword, tokens[2].subtype);
        EXPECT_EQ(kw_op, tokens[2].kw);
        EXPECT_EQ(tt_literal, tokens[4].type);
        EXPECT_EQ(tt_sub_keyword, tokens[4].subtype);
        EXPECT_EQ(kw_in, tokens[4].kw);
        EXPECT_EQ(tt_literal, tokens[6].type);
        EXPECT_EQ(tt_sub_keyword, tokens[6].subtype);
        EXPECT_EQ(kw_graph, tokens[6].kw);
        EXPECT_EQ(tt_literal, tokens[8].type);
        EXPECT_EQ(tt_sub_none, tokens[8].subtype);
    }

    TEST(TokenizerTest, Literal) {
        auto tokens = tokenize_str("'good'use arg");
        ASSERT_EQ(4, tokens.size());
        EXPECT_EQ(tt_literal, tokens[0].type);
        EXPECT_EQ(tt_sub_string, tokens[0].subtype);
        EXPECT_STREQ("'good'", tokens[0].text.c_str());
        EXPECT_STREQ("good", tokens[0].parsed.c_str());
        EXPECT_EQ(tt_literal, tokens[1].type);
        EXPECT_EQ(tt_sub_none, tokens[1].subtype);
        EXPECT_EQ(tt_literal, tokens[3].type);
        EXPECT_EQ(tt_sub_keyword, tokens[3].subtype);
        EXPECT_EQ(kw_arg, tokens[3].kw);
    }
}
