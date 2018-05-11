#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "dp/operators.h"
#include "dp/types.h"

namespace dp::op {
    using namespace std;
    using namespace cv;

    void decode_image::operator() (graph::ctx ctx) {
        const buf_ref& buf = ctx.in(0)->as<buf_ref>();
        auto img = imdecode(Mat(1, buf.len, CV_8UC1, buf.ptr), 1);
        ctx.out(0)->set<Mat>(img);
        ctx.out(1)->set<image_size>(image_size(img.cols, img.rows));
    }
}
