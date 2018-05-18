#include "dp/types.h"
#include "dp/operators.h"

namespace dp::op {
    using namespace std;

    void detect_boxes_json::operator() (graph::ctx ctx) {
        const auto& sz = ctx.in(0)->as<dp::image_size>();
        const auto& id = ctx.in(1)->as<dp::image_id>();
        const auto& boxes = ctx.in(2)->as<vector<dp::detect_box>>();
        char cstr[1024];
        sprintf(cstr, "{\"src\":\"%s\",\"seq\":%lu,\"size\":[%d,%d],\"boxes\":[",
            id.src.c_str(), id.seq, sz.w, sz.h);
        string str(cstr);
        for (auto& b : boxes) {
            sprintf(cstr, "{\"class\":%d,\"score\":%f,\"rc\":[%d,%d,%d,%d]},",
                b.category, b.confidence, b.x0, b.y0, b.x1, b.y1);
            str += cstr;
        }
        str = str.substr(0, str.length()-1) + "]}";
        ctx.out(0)->set<string>(str);
    }
}
