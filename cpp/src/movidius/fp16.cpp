#include <mvnc.h>
#include "movidius/ncs.h"

namespace movidius {
    float half2float(uint16_t fp16) {
        uint32_t e = fp16 & 0x7c00u;
        uint32_t sgn = ((uint32_t)fp16 & 0x8000u) << 16;
        uint32_t bits = 0;

        switch (e) {
        case 0:
            bits = fp16 & 0x03ffu;
            if (bits == 0) bits = sgn;
            else {
                bits <<= 1;
                while ((bits & 0x0400) == 0) {
                    bits <<= 1;
                    e ++;
                }
                e = ((uint32_t)(127 - 15 - e) << 23);
                bits = (bits & 0x03ff) << 13;
                bits += e + sgn;
            }
            break;
        case 0x7c00u:
            bits = sgn + 0x7f800000u + (((uint32_t)fp16 & 0x03ffu) << 13);
            break;
        default:
            bits = sgn + ((((uint32_t)fp16 & 0x7fffu) + 0x1c000u) << 13);
            break;
        }
        return *(float*)&bits;
    }

    uint16_t float2half(float fp32) {
        uint32_t u32 = *(uint32_t*)&fp32;

        // sign
        uint16_t sgn = (uint16_t)((u32 & 0x80000000) >> 16);
        // exponent
        uint32_t e = u32 & 0x7f800000;

        if (e >= 0x47800000) {
            return sgn | 0x7c00;
        } else if (e < 0x33000000) {
            return sgn;
        } else if (e <= 0x38000000) {
            e >>= 23;
            uint32_t bits = (0x00800000 + (u32 & 0x007fffff)) >> (113 - e);
            return sgn | (uint16_t)((bits + 0x00001000) >> 13);
        }

        return sgn +
            (uint16_t)((e - 0x38000000) >> 13) +
            (uint16_t)(((u32 & 0x007fffff) + 0x00001000) >> 13);
    }
}
