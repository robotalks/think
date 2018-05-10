#ifndef __VP_CV_OPERATORS_H
#define __VP_CV_OPERATORS_H

#include "vp/graph.h"

namespace vp {
    struct CvImageDecode {
        struct Op {
            void operator() (Graph::Ctx);
        };

        struct Factory {
            Graph::OpFunc operator()() const { return Op(); }
        };
    };
}

#endif
