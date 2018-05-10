#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <mvnc.h>

#include "movidius/ncs.h"

namespace movidius {
    using namespace std;

    void SSDMobileNet::Op::operator() (vp::Graph::Ctx ctx) {
        cv::Mat m = ctx.in(0)->as<cv::Mat>();
        int s = max(m.rows, m.cols);
        if (s != ImageSz) {
            cv::Mat d;
            double f = (double)ImageSz/s;
            cv::resize(m, d, cv::Size(), f, f);
            m = d;
        }
        cv::Mat m16 = CropFp16Op::cropFp16(m, ImageSz, ImageSz);
        graph->exec(m16.data, m16.total() * 6, [ctx] (const void* out, size_t len) {
            const cv::Mat& m = ctx.in(0)->as<cv::Mat>();
            ctx.out(0)->set<vector<vp::DetectBox>>(
                move(SSDMobileNet::parseResult(out, len, m.cols, m.rows)));
        });
    }

    vector<vp::DetectBox> SSDMobileNet::parseResult(
        const void* out, size_t len, int origCX, int origCY) {
        int s = max(origCX, origCY);
        int offx = (origCX - s)/2;
        int offy = (origCY - s)/2;
        vector<vp::DetectBox> boxes;
        auto fp16 = (const uint16_t*)out;
        len >>= 1;
        if (len > 7) {
            int count = (int)half2float(fp16[0]);
            if (count < 0) count = 0;
            if (count > 1000) count = 1000;
            for (int i = 0; i < count; i ++) {
                int off = (i+1)*7;
                if (off + 7 > len) break;
                vp::DetectBox box;
                box.category = (int)half2float(fp16[off+1]);
                box.confidence = half2float(fp16[off+2]);
                box.x0 = offx+(int)(half2float(fp16[off+3])*s);
                box.y0 = offy+(int)(half2float(fp16[off+4])*s);
                box.x1 = offx+(int)(half2float(fp16[off+5])*s);
                box.y1 = offy+(int)(half2float(fp16[off+6])*s);
                boxes.push_back(box);
            }
        }
        return boxes;
    }
}
