#include "dp/types.h"
#include "dp/operators.h"
#include "dp/graph_def.h"

namespace dp::op {
    using namespace std;

    struct wrap_factory : public dp::graph_def::op_factory {
        function<dp::graph::op_func(
            const string&, const string&, 
            const dp::graph_def::params&)> func;

        wrap_factory(function<dp::graph::op_func(
            const string&, const string&, 
            const dp::graph_def::params&)> f) : func(f) {}

        dp::graph::op_func create_op(
            const string& name, const string& type,
            const dp::graph_def::params& args) {
            return func(name, type, args);
        }
    };

    template<typename T>
    struct noparams_factory : public dp::graph_def::op_factory {
        dp::graph::op_func create_op(
            const string& name, const string& type,
            const dp::graph_def::params& args) {
            return T();
        }
    };

    void register_factories() {
        static noparams_factory<image_id> imageid_f;
        static noparams_factory<decode_image> decodeimg_f;
        static wrap_factory saveimg_f(
            [] (const string& name, const string& type,
                const dp::graph_def::params& args) {
                string prefix("img"), suffix(".jpg");
                auto it = args.find("fn.prefix");
                if (it != args.end()) {
                    prefix = it->second;
                }
                if ((it = args.find("fn.suffix")) != args.end()) {
                    suffix = it->second;
                }
                return save_image(prefix, suffix);
            }
        );
        static noparams_factory<detect_boxes_json> detectboxesjson_f;

        auto reg = dp::graph_def::op_registry::get();
        reg->add_factory("dp.image_id", &imageid_f);
        reg->add_factory("dp.decode_image", &decodeimg_f);
        reg->add_factory("dp.save_image", &saveimg_f);
        reg->add_factory("dp.detect_boxes_json", &detectboxesjson_f);
    }
}
