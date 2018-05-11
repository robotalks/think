#include <memory>
#include <thread>

#include "movidius/ncs.h"
#include "movidius/operators.h"

namespace movidius::op {
    using namespace std;

    void exec::operator() (dp::graph::ctx ctx) {
        const cv::Mat& m = ctx.in(0)->as<cv::Mat>();
        graph->exec(m.data, m.rows * m.cols * 6, [ctx] (const void* out, size_t len) {
            len >>= 1;
            vector<float> fp(len);
            auto fp16 = (const uint16_t*)out;
            for (size_t i = 0; i < len; i ++) {
                fp[i] = half2float(fp16[i]);
            }
            ctx.out(0)->set<vector<float>>(move(fp));
        });
    }

    static void crop16fp_rows(const cv::Mat& src, cv::Mat& dst, int start, int rows) {
        int srcr0 = (src.rows - dst.rows) / 2;
        int srcc0 = (src.cols - dst.cols) / 2;
        for (int r = 0; r < rows; r ++) {
            uint16_t *p = (uint16_t*)dst.ptr(start+r);
            int sr = srcr0 + start + r;
            if (sr < 0 || sr >= src.rows) {
                memset(p, 0, dst.cols*2*3);
            } else {
                int sc = srcc0;
                for (int c = 0; c < dst.cols; c ++) {
                    if (sc < 0 || sc >= src.cols) {
                        memset(p, 0, 6);
                    } else {
                        uint8_t *sp = (uint8_t*)src.ptr(sr, sc);
                        for (int i = 0; i < 3; i ++)
                            p[i] = float2half(((float)(uint32_t)(sp[i]) - 127.5) * 0.007843);
                    }
                    sc ++;
                    p += 3;
                }
            }
        }
    }

    void crop_fp16::operator() (dp::graph::ctx ctx) {
        const cv::Mat& m = ctx.in(0)->as<cv::Mat>();
        ctx.out(0)->set<cv::Mat>(crop(m));
    }

    cv::Mat crop_fp16::crop(const cv::Mat &m, int cx, int cy) {
        cv::Mat m16(cy, cx, CV_16UC3);
        vector<unique_ptr<thread>> threads;
        int conc = 4;
        int rows = cy / conc;
        if (cy % conc) rows ++;
        for (int i = 0; i < conc; i ++) {
            int s = rows * i, n = rows;
            if (s + n > cy) n = cy - s;
            auto t = new thread([&m, &m16, s, n] () {
                crop16fp_rows(m, m16, s, n);
            });
            threads.push_back(move(unique_ptr<thread>(t)));
        }
        for (auto& pt : threads) pt->join();
        return m16;
    }
}
