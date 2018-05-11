#include "dp/types.h"
#include "dp/operators.h"

namespace dp::op {
    using namespace std;

    void image_id::operator() (graph::ctx ctx) {
        dp::image_id id;
        dp::image_id::extract(ctx.in(0)->as<buf_ref>(), id);
        ctx.out(0)->set<dp::image_id>(id);
    }
}
