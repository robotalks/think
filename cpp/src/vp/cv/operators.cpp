#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "vp/operators.h"
#include "vp/cv/operators.h"

namespace vp {
    using namespace std;
    using namespace cv;

    void CvImageDecode::Op::operator() (Graph::Ctx ctx) {
        const BufRef& buf = ctx.in(0)->as<BufRef>();
        auto img = imdecode(Mat(1, buf.len, CV_8UC1, buf.ptr), 1);
        ctx.out(0)->set<Mat>(img);
    }
}
