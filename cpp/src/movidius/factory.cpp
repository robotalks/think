#include <stdexcept>
#include "dp/graph_def.h"
#include "movidius/ncs.h"
#include "movidius/operators.h"
#include "movidius/ssd_mobilenet.h"

namespace movidius::op {
    using namespace std;

    static movidius::device_pool _devices;

    struct graph_pool {
        struct entry {
            compute_stick* stick;
            compute_stick::graph* graph;
        };

        graph_pool() {}

        ~graph_pool() {
            for (auto& ent : entries) {
                delete ent.graph;
                _devices.release(ent.stick);
            }
        }

        list<entry> entries;

        entry& alloc(const string& model, const string& dev) {
            entry ent;
            ent.stick = _devices.alloc(dev);
            if (ent.stick == nullptr) {
                throw runtime_error("device unavailable");
            }
            try {
                ent.graph = ent.stick->alloc_graph_from_file(model);
            } catch (const exception&) {
                _devices.release(ent.stick);
                throw;
            }
            entries.push_back(ent);
            return entries.back();
        }
    };

    static graph_pool _graphs;

    template<typename T>
    struct graph_op_factory : public dp::graph_def::op_factory {
        dp::graph::op_func create_op(
            const string& name,
            const string& type,
            const dp::graph_def::params& args) {
            auto model_name = args.at("model");
            if (model_name.empty()) {
                model_name = "graph";
            }
            auto ent = _graphs.alloc(model_name, args.at("device"));
            return [ent] (dp::graph::ctx ctx) {
                T(ent.graph)(ctx);
            };
        }
    };

    struct wrap_op_factory : public dp::graph_def::op_factory {
        using create_op_func = function<dp::graph::op_func(const dp::graph_def::params&)>;
        create_op_func func;
        wrap_op_factory(create_op_func f) : func(f) { }
        dp::graph::op_func create_op(
            const string& name, const string& type,
            const dp::graph_def::params& params) {
            return func(params);
        };
    };

    static wrap_op_factory _cropfp16_factory(
        [] (const dp::graph_def::params& args) -> dp::graph::op_func {
        auto cx_str = args.at("cx");
        if (cx_str.empty()) throw invalid_argument("missing required parameter: cx");
        auto cy_str = args.at("cy");
        if (cy_str.empty()) throw invalid_argument("missing required parameter: cy");
        int cx = atoi(cx_str.c_str()), cy = atoi(cy_str.c_str());
        if (cx <= 0) throw invalid_argument("parameter cx must be positive");
        if (cy <= 0) throw invalid_argument("parameter cy must be positive");
        return [cx, cy] (dp::graph::ctx ctx) {
            crop_fp16(cx, cy)(ctx);
        };
    });

    static graph_op_factory<exec> _exec_factory;
    static graph_op_factory<ssd_mobilenet> _ssd_mobilenet_factory;

    void register_factories() {
        _devices.populate();
        auto reg = dp::graph_def::op_registry::get();
        reg->add_factory("mvnc.crop_fp16", &_cropfp16_factory);
        reg->add_factory("mvnc.exec", &_exec_factory);
        reg->add_factory("mvnc.ssd_mobilenet", &_ssd_mobilenet_factory);
    }
}
