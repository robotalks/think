#include <vector>
#include "gtest/gtest.h"

#include "graph_def_parse.h"

namespace dp {
    using namespace std;

    ast parse_str(const string& str) {
        parser p;
        p.add(tokenizer().add(str, true));
        return move(p.end());
    }

    TEST(ParserTest, Empty) {
        auto ast = parse_str(R"()");
        EXPECT_EQ(0, ast.graphs.size());
    }

    TEST(ParserTest, Graph) {
        auto ast = parse_str("arg in\ngraph a");
        ASSERT_EQ(2, ast.graphs.size());
        auto g = ast.graphs.begin();
        EXPECT_STREQ("", g->name.c_str());
        g ++;
        EXPECT_STREQ("a", g->name.c_str());
        EXPECT_STREQ("a", g->name_iter->parsed.c_str());

        ast = parse_str("graph b");
        ASSERT_EQ(1, ast.graphs.size());
        EXPECT_STREQ("b", ast.graphs.front().name.c_str());
    }

    TEST(ParserTest, ArgumentList) {
        auto ast = parse_str(R"(arg input)");
        ASSERT_EQ(1, ast.graphs.size());
        auto g = ast.graphs.begin();
        EXPECT_STREQ("", g->name.c_str());
        ASSERT_EQ(1, g->args.size());
        EXPECT_STREQ("input", g->args.front().name->parsed.c_str());
        EXPECT_EQ(nullptr, g->args.front().value);

        ast = parse_str(R"(arg in1, in2 = hello)");
        ASSERT_EQ(1, ast.graphs.size());
        g = ast.graphs.begin();
        ASSERT_EQ(2, g->args.size());
        auto it = g->args.begin();
        EXPECT_STREQ("in1", it->name->parsed.c_str());
        EXPECT_EQ(nullptr, it->value);
        it ++;
        EXPECT_STREQ("in2", it->name->parsed.c_str());
        EXPECT_TRUE(it->value != nullptr);
        EXPECT_STREQ("hello", it->value->parsed.c_str());

        ast = parse_str("arg in1, #comment\nin2 = hello\narg in3 = 'world'");
        ASSERT_EQ(1, ast.graphs.size());
        g = ast.graphs.begin();
        ASSERT_EQ(3, g->args.size());
        it = g->args.begin();
        EXPECT_STREQ("in1", it->name->parsed.c_str());
        EXPECT_EQ(nullptr, it->value);
        it ++;
        EXPECT_STREQ("in2", it->name->parsed.c_str());
        EXPECT_TRUE(it->value != nullptr);
        EXPECT_STREQ("hello", it->value->parsed.c_str());
        it ++;
        EXPECT_STREQ("in3", it->name->parsed.c_str());
        EXPECT_TRUE(it->value != nullptr);
        EXPECT_STREQ("world", it->value->parsed.c_str());
    }

    TEST(ParserTest, Ingress) {
        auto ast = parse_str(R"(in input = udp use ingress.udp{port:1234})");
        ASSERT_EQ(1, ast.graphs.size());
        ASSERT_EQ(1, ast.graphs.front().ingresses.size());
        auto in = ast.graphs.front().ingresses.begin();
        EXPECT_STREQ("udp", in->name->parsed.c_str());
        EXPECT_STREQ("ingress.udp", in->factory->parsed.c_str());
        ASSERT_EQ(1, in->input_vars.size());
        EXPECT_STREQ("input", in->input_vars.front().name->parsed.c_str());
        EXPECT_EQ(var_in, in->input_vars.front().as);
        ASSERT_EQ(1, in->params.size());
        EXPECT_STREQ("port", in->params.front().key->parsed.c_str());
        EXPECT_STREQ("1234", in->params.front().val->parsed.c_str());

        ast = parse_str(R"(in in1, in2 = udp use ingress.udp{port:1234})");
        ASSERT_EQ(1, ast.graphs.size());
        ASSERT_EQ(1, ast.graphs.front().ingresses.size());
        in = ast.graphs.front().ingresses.begin();
        ASSERT_EQ(2, in->input_vars.size());
        auto it = in->input_vars.begin();
        EXPECT_STREQ("in1", it->name->parsed.c_str());
        it ++;
        EXPECT_STREQ("in2", it->name->parsed.c_str());
    }

    TEST(ParserTest, Operators) {
        auto ast = parse_str(
            "op o1 = op1(in1) use optest{k1:val1,k2:val2}\n"
            "o2, o3 = op2(in1, in2) use optest{}\n"
            "op3(in3) use optest{}");
        ASSERT_EQ(1, ast.graphs.size());
        ASSERT_EQ(3, ast.graphs.front().ops.size());
        auto op = ast.graphs.front().ops.begin();
        EXPECT_STREQ("op1", op->name->parsed.c_str());
        EXPECT_STREQ("optest", op->factory->parsed.c_str());
        ASSERT_EQ(1, op->o_vars.size());
        ASSERT_EQ(1, op->i_vars.size());
        EXPECT_STREQ("o1", op->o_vars.front().name->parsed.c_str());
        EXPECT_EQ(var_out, op->o_vars.front().as);
        EXPECT_STREQ("in1", op->i_vars.front().name->parsed.c_str());
        EXPECT_EQ(var_in, op->i_vars.front().as);
        ASSERT_EQ(2, op->params.size());
        auto it = op->params.begin();
        EXPECT_STREQ("k1", it->key->parsed.c_str());
        EXPECT_STREQ("val1", it->val->parsed.c_str());
        it ++;
        EXPECT_STREQ("k2", it->key->parsed.c_str());
        EXPECT_STREQ("val2", it->val->parsed.c_str());

        op ++;
        EXPECT_STREQ("op2", op->name->parsed.c_str());
        ASSERT_EQ(2, op->o_vars.size());
        ASSERT_EQ(2, op->i_vars.size());
        auto v = op->o_vars.begin();
        EXPECT_STREQ("o2", v->name->parsed.c_str());
        EXPECT_EQ(var_out, v->as);
        v ++;
        EXPECT_STREQ("o3", v->name->parsed.c_str());
        EXPECT_EQ(var_out, v->as);
        ASSERT_EQ(2, op->i_vars.size());
        v = op->i_vars.begin();
        EXPECT_STREQ("in1", v->name->parsed.c_str());
        EXPECT_EQ(var_in, v->as);
        v ++;
        EXPECT_STREQ("in2", v->name->parsed.c_str());
        EXPECT_EQ(var_in, v->as);

        op ++;
        EXPECT_STREQ("op3", op->name->parsed.c_str());
        ASSERT_EQ(0, op->o_vars.size());
        ASSERT_EQ(1, op->i_vars.size());
        v = op->i_vars.begin();
        EXPECT_STREQ("in3", v->name->parsed.c_str());
        EXPECT_EQ(var_in, v->as);
    }

    TEST(ParserTest, FullSimple) {
        auto ast = parse_str(R"(
            in input = test use ingress.test{value: "in1"}
            out = test(input) use op.test{value: "res"}
        )");
        ASSERT_EQ(1, ast.graphs.size());
        ASSERT_EQ(1, ast.graphs.front().ingresses.size());
        auto in = ast.graphs.front().ingresses.begin();
        EXPECT_STREQ("test", in->name->parsed.c_str());
        EXPECT_STREQ("ingress.test", in->factory->parsed.c_str());
        ASSERT_EQ(1, in->input_vars.size());
        EXPECT_STREQ("input", in->input_vars.front().name->parsed.c_str());
        EXPECT_EQ(var_in, in->input_vars.front().as);
        ASSERT_EQ(1, in->params.size());
        EXPECT_STREQ("value", in->params.front().key->parsed.c_str());
        EXPECT_STREQ("in1", in->params.front().val->parsed.c_str());
        ASSERT_EQ(1, ast.graphs.front().ops.size());
        auto op = ast.graphs.front().ops.begin();
        EXPECT_STREQ("test", op->name->parsed.c_str());
        EXPECT_STREQ("op.test", op->factory->parsed.c_str());
        ASSERT_EQ(1, op->o_vars.size());
        ASSERT_EQ(1, op->i_vars.size());
        EXPECT_STREQ("out", op->o_vars.front().name->parsed.c_str());
        EXPECT_EQ(var_out, op->o_vars.front().as);
        EXPECT_STREQ("input", op->i_vars.front().name->parsed.c_str());
        EXPECT_EQ(var_in, op->i_vars.front().as);
        ASSERT_EQ(1, op->params.size());
        EXPECT_STREQ("value", op->params.front().key->parsed.c_str());
        EXPECT_STREQ("res", op->params.front().val->parsed.c_str());
    }
}
