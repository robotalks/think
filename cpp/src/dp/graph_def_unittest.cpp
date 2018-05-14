#include <vector>
#include "gtest/gtest.h"

#include "dp/util/queue.h"
#include "graph_def_parse.h"

namespace dp {
    using namespace std;

    struct test_op_factory : public graph_def::op_factory {
        graph::op_func create_op(const string& name, const string& type, 
            const graph_def::params& params) {
            return [name, type, params] (graph::ctx ctx) {
                ctx.out(0)->set(ctx.in(0)->as<string>() + params.at("value"));
            };
        }
    };

    struct test_ingress_factory;

    struct test_ingress : public ingress {
        test_ingress_factory *factory;
        string name;
        string type;
        graph_def::params params;

        bool recv(dispatcher*, bool nowait);

        test_ingress(test_ingress_factory* f, const string& n, const string& t, const graph_def::params& p)
        : factory(f), name(n), type(t), params(p) {
        }
    };

    struct test_ingress_factory : public graph_def::ingress_factory {
        queue<string> in;
        queue<string> out;

        ingress* create_ingress(const string& name, const string& type,
            const graph_def::params& params) {
            return new test_ingress(this, name, type, params);
        }
    };

    bool test_ingress::recv(dispatcher *disp, bool nowait) {
        auto val = factory->in.get();
        if (val.empty()) return false;
        session s;
        s.initializer = [this, val] (graph* g) {
            g->var(input_var())->set<string>(val);
        };
        s.finalizer = [this] (graph* g) {
            factory->out.put(g->var("out")->as<string>());
        };
        return disp->dispatch(s, nowait);
    }

    struct test_ctx {
        test_op_factory op_f;
        test_ingress_factory ing_f;
        graph_def::op_registry op_r;
        graph_def::ingress_registry in_r;

        graph_def def;
        exec_env env;

        test_ctx(const string& script) {
            op_r.add_factory("test", &op_f);
            in_r.add_factory("test", &ing_f);
            def.use_ops(&op_r)
                .use_ingresses(&in_r)
                .parse(script);
        }

        test_ctx& build(const graph_def::params args = {}) {
            def.build_env(env, args);
            return *this;
        }

        test_ctx& build(const string& name, const graph_def::params& args = {}) {
            def.select_graph(name).build_env(env, args);
            return *this;
        }

        test_ctx& start() {
            env.start();
            return *this;
        }

        string run(const string& input) {
            ing_f.in.put(input);
            return ing_f.out.get();
        }

        test_ctx& stop() {
            env.stop();
            ing_f.in.put("");
            env.runner.join();
        }
    };

    TEST(GraphDefTest, SimpleGraph) {
        test_ctx ctx(R"(
            in input = test use test{}
            tmp = test_tmp(input) use test{value: "."}
            out = test_out(tmp) use test{value: "res"}
        )");
        ctx.build().start();
        EXPECT_STREQ("t1.res", ctx.run("t1").c_str());
        EXPECT_STREQ("t2.res", ctx.run("t2").c_str());
        ctx.stop();
    }
}
