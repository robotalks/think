#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "movidius/ncs.h"
#include "movidius/operators.h"
#include "movidius/ssd_mobilenet.h"

namespace movidius::op {
    using namespace std;

    void ssd_mobilenet::operator() (dp::graph::ctx ctx) {
        cv::Mat m = ctx.in(0)->as<cv::Mat>();
        int s = max(m.rows, m.cols);
        if (s != net_imagesz) {
            cv::Mat d;
            double f = (double)net_imagesz/s;
            cv::resize(m, d, cv::Size(), f, f);
            m = d;
        }
        cv::Mat m16 = crop_fp16::crop(m, net_imagesz, net_imagesz);
        graph->exec(m16.data, m16.total() * 6, [ctx] (const void* out, size_t len) {
            const cv::Mat& m = ctx.in(0)->as<cv::Mat>();
            ctx.out(0)->set<vector<dp::detect_box>>(
                move(to_detect_boxes(out, len, dp::image_size(m.cols, m.rows))));
        });
    }

    vector<dp::detect_box> ssd_mobilenet::to_detect_boxes(
        const void* out, size_t len, const dp::image_size& orig) {
        int s = max(orig.w, orig.h);
        int offx = (orig.w - s)/2;
        int offy = (orig.h - s)/2;
        vector<dp::detect_box> boxes;
        auto fp16 = (const uint16_t*)out;
        len >>= 1;
        if (len > 7) {
            int count = (int)half2float(fp16[0]);
            if (count < 0) count = 0;
            if (count > 1000) count = 1000;
            for (int i = 0; i < count; i ++) {
                int off = (i+1)*7;
                if (off + 7 > len) break;
                dp::detect_box box;
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
