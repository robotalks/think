#include "vp/operators.h"

namespace vp {
    using namespace std;

    static float norm(float v) {
        return v < 0 ? 0 : ((v > 1) ? 1 : v);
    }

    float Sensitivity::Mat::level(const ImageSize& sz, const DetectBox& b) const {
        if (sz.w == 0 || sz.h == 0 || size() < DIMS) {
            return 0.0;
        }
        float w = (float)(b.x1-b.x0)/(float)sz.w;
        float h = (float)(b.y1-b.y0)/(float)sz.h;
        const vector<float>& m = *this;
        return m[0]*norm(m[1]*w*w + m[2]*w + m[3]) +
               m[4]*norm(m[5]*h*h + m[6]*h + m[7]) +
               b.confidence*norm(m[8]);
    }

    void Sensitivity::Op::operator() (Graph::Ctx ctx) {
        const ImageSize& sz = ctx.in(0)->as<ImageSize>();
        const vector<DetectBox>& boxes = ctx.in(2)->as<vector<DetectBox>>();
        vector<float> levels;
        for (auto& b : boxes) {
            if (b.category >= 0 && b.category < categories.size()) {
                levels.push_back(categories[b.category].level(sz, b));
            } else {
                levels.push_back(0.0);
            }
        }
        ctx.out(0)->set<vector<float>>(move(levels));
    }
}
