#ifndef __MOVIDIUS_OPERATORS_H
#define __MOVIDIUS_OPERATORS_H

#include <string>
#include <vector>
#include <functional>
#include <opencv2/core/core.hpp>

#include "dp/graph.h"
#include "movidius/ncs.h"

namespace movidius::op {
    struct exec {
        compute_stick::graph *graph;
        exec(compute_stick::graph *g) : graph(g) { }
        void operator() (::dp::graph::ctx);
    };

    struct crop_fp16 {
        int cx, cy;
        crop_fp16(int _cx, int _cy) : cx(_cx), cy(_cy) { }
        void operator() (::dp::graph::ctx);

        ::cv::Mat crop(const ::cv::Mat& m) {
            return crop(m, cx, cy);
        }

        static ::cv::Mat crop(const ::cv::Mat&, int cx, int cy);
    };

    void register_factories();
}

#endif
