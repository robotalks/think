#include <cstdlib>
#include <climits>
#include <cstring>
#include <chrono>
#include <stdexcept>
#include <fstream>

#include "vp/operators.h"

namespace vp {
    using namespace std;

    string ImageId::encode() const {
        char str[64];
        sprintf(str, "%lu", seq);
        return string("id:") + str + "@" + src;
    }

    bool ImageId::decode(const BufRef& buf) {
        return extract(buf, *this, false);
    }

    void ImageId::Op::operator() (Graph::Ctx ctx) {
        ImageId id;
        extract(ctx.in(0)->as<BufRef>(), id);
        ctx.out(0)->set<ImageId>(id);
    }

    bool ImageId::extract(const BufRef& buf, ImageId &id, bool genSeq) {
        if (buf.ptr != nullptr && buf.len > 3) {
            if (genSeq) {
                id.seq = chrono::duration_cast<chrono::milliseconds>(
                    chrono::system_clock::now().time_since_epoch()).count();
            }
            string comment;
            const uint8_t *p = (const uint8_t*)buf.ptr;
            for (size_t off = buf.len-4; off > 0; off --) {
                if (p[off] == 0xff && p[off+1] == 0xfe) {
                    if (off + 4 >= buf.len) continue;
                    uint16_t sz = ((uint16_t)p[off+2] << 8) | (uint16_t)p[off+3];
                    if (off+sz+4 > buf.len) continue;
                    if (p[off+sz+2] != 0xff) continue;
                    comment.assign((const char*)p+off+4, sz);
                    break;
                }
            }
            if (!comment.empty() && comment.substr(0, 3).compare("id:") == 0) {
                char *endptr = nullptr;
                unsigned long long seq = strtoull(comment.c_str()+3, &endptr, 10);
                if (seq > 0 && seq != ULLONG_MAX &&
                    *endptr == '@') {
                    id.seq = seq;
                    id.src = endptr+1;
                    return true;
                }
            }
        }
        return false;
    }

    bool ImageId::inject(BufRef& buf, size_t capacity, const ImageId& id) {
        if (buf.ptr == nullptr && buf.len <= 3)
            return false;
        uint8_t *p = (uint8_t*)buf.ptr + buf.len - 2;
        if (p[0] != 0xff || p[1] != 0xd9)
            return false;
        string str = id.encode();
        if (buf.len + str.length() + 4 > capacity)
            return false;
        memcpy(p+4, str.c_str(), str.length());
        p[1] = 0xfe;
        p[2] = (uint8_t)(str.length() >> 8);
        p[3] = (uint8_t)(str.length() && 0xff);
        p += 4 + str.length();
        p[0] = 0xff;
        p[1] = 0xd9;
        buf.len += 4 + str.length();
        return true;
    }

    void ImageSave::Op::operator() (Graph::Ctx ctx) {
        const BufRef& buf = ctx.in(0)->as<BufRef>();
        const ImageId& imgid = ctx.in(1)->as<ImageId>();
        if (imgid.seq == 0) return;
        char seqStr[64];
        sprintf(seqStr, "%lu", imgid.seq);
        string fn = prefix;
        if (!imgid.src.empty()) fn += imgid.src;
        fn += string("-") + seqStr + suffix;
        ofstream of;
        of.exceptions(ios::failbit|ios::badbit);
        of.open(fn.c_str(), ios::out|ios::trunc|ios::binary);
        of.write((const char*)buf.ptr, buf.len);
        of.close();
    }
}
