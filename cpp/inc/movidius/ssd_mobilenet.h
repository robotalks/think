#ifndef __MOVIDIUS_SSD_MOBILENET_H
#define __MOVIDIUS_SSD_MOBILENET_H

#include <vector>

#include "dp/graph.h"
#include "dp/types.h"
#include "movidius/ncs.h"

namespace movidius::op {
    struct ssd_mobilenet {
        // image w and h
        static constexpr int net_imagesz = 300;

        compute_stick::graph *graph;
        ssd_mobilenet(compute_stick::graph *g) : graph(g) { }
        void operator() (::dp::graph::ctx);

        static ::std::vector<::dp::detect_box> to_detect_boxes(
            const void* out, size_t len, const ::dp::image_size& orig);
    };
}

#endif
