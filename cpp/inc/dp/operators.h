#ifndef __DP_OPERATORS_H
#define __DP_OPERATORS_H

#include <string>

#include "dp/graph.h"
#include "dp/types.h"

namespace dp::op {
    struct wrap {
        graph::op_func fn;
        wrap(const graph::op_func& f) : fn(f) { }
        void operator() (graph::ctx ctx) { fn(ctx); }
        // factory
        graph::op_func operator()() const { return fn; }
    };

    struct image_id {
        void operator() (graph::ctx);
        // factory
        graph::op_func operator()() const { return *const_cast<image_id*>(this); }
    };

    struct decode_image {
        void operator() (graph::ctx);
        // factory
        graph::op_func operator()() const { return *const_cast<decode_image*>(this); }
    };

    struct save_image {
        ::std::string prefix, suffix;
        save_image(const ::std::string& _prefix = "img",
            const ::std::string& _suffix = ".jpg")
        : prefix(_prefix), suffix(_suffix) { }
        void operator() (graph::ctx);
        // factory
        graph::op_func operator()() const { return *const_cast<save_image*>(this); }
    };

    struct detect_boxes_json {
        void operator() (graph::ctx);
    };

    struct sensitivity {
        struct mat : public ::std::vector<float> {
            // dimensions:
            // w_weight, a, b, c
            // h_weight, a, b, c
            // confidence_weight
            // w_weight + h_weight + confidence_weight = 1
            // sensitivity level = w_weight * max(1, min(0, (a*w*w + b*w + c)))
            //                   + h_weight * max(1, min(0, (a*h*h + b*h + c)))
            //                   + confidence_weight * confidence
            static constexpr int dims = 9;
            mat() : ::std::vector<float>(dims, 0.0) { (*this)[dims-1] = 1.0; }
            mat(const ::std::vector<float>& v) : ::std::vector<float>(v) { resize(dims); }
            mat(::std::vector<float>&& v) : ::std::vector<float>(v) { resize(dims); }

            float level(const image_size&, const detect_box&) const;
        };

        ::std::vector<mat> categories;
        sensitivity(const ::std::vector<mat>& _cats) : categories(_cats) {}
        void operator() (graph::ctx);
        // factory
        graph::op_func operator() () const { return *const_cast<sensitivity*>(this); }
    };

    void register_factories();
}

#endif
