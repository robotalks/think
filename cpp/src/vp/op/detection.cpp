#include <opencv2/core/core.hpp>

#include "vp/operators.h"

namespace vp {
    using namespace std;

    void DetectBoxesPub::Op::operator() (Graph::Ctx ctx) {
        const cv::Mat& m = ctx.in(0)->as<cv::Mat>();
        const ImageId& id = ctx.in(1)->as<ImageId>();
        const vector<DetectBox>& boxes = ctx.in(2)->as<vector<DetectBox>>();
        char cstr[1024];
        sprintf(cstr, "{\"src\":\"%s\",\"seq\":%lu,\"size\":[%d,%d],\"boxes\":[",
            id.src.c_str(), id.seq, m.cols, m.rows);
        string str(cstr);
        for (auto& b : boxes) {
            sprintf(cstr, "{\"class\":%d,\"score\":%f,\"rc\":[%d,%d,%d,%d]},",
                b.category, b.confidence, b.x0, b.y0, b.x1, b.y1);
            str += cstr;
        }
        str = str.substr(0, str.length()-1) + "]}";
        client->publish(topic, str);
    }
}
