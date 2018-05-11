#ifndef __DP_TYPES_H
#define __DP_TYPES_H

#include <string>

namespace dp {
    struct buf_ref {
        void* ptr;
        size_t len;
    };

    struct image_size {
        int w, h;

        image_size() : w(0), h(0) { }
        image_size(int _w, int _h) : w(_w), h(_h) { }
    };

    struct image_id {
        uint64_t seq;
        ::std::string src;

        image_id() : seq(0) { }

        ::std::string encode() const;
        bool decode(const buf_ref&);

        static bool extract(const buf_ref&, image_id&, bool gen_seq = true);
        static bool inject(buf_ref&, size_t capacity, const image_id&);
    };

    struct detect_box {
        int   category;
        float confidence;
        int   x0, y0, x1, y1;
    };
}

#endif
