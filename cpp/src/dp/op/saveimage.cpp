#include <cstring>
#include <stdexcept>
#include <fstream>

#include "dp/types.h"
#include "dp/operators.h"

namespace dp::op {
    using namespace std;

    void save_image::operator() (graph::ctx ctx) {
        const buf_ref& buf = ctx.in(0)->as<buf_ref>();
        const dp::image_id& imgid = ctx.in(1)->as<dp::image_id>();
        if (imgid.seq == 0) return;
        char seq_str[64];
        sprintf(seq_str, "%lu", imgid.seq);
        string fn = prefix;
        if (!imgid.src.empty()) fn += imgid.src;
        fn += string("-") + seq_str + suffix;
        ofstream of;
        of.exceptions(ios::failbit|ios::badbit);
        of.open(fn.c_str(), ios::out|ios::trunc|ios::binary);
        of.write((const char*)buf.ptr, buf.len);
        of.close();
    }
}
