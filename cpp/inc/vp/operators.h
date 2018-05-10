#ifndef __VP_OPERATORS_H
#define __VP_OPERATORS_H

#include <string>
#include "vp/graph.h"
#include "vp/mqtt/mqtt.h"

namespace vp {
    struct WrapOp {
        Graph::OpFunc fn;
        WrapOp(const Graph::OpFunc& f) : fn(f) { }
        Graph::OpFunc operator()() const { return fn; }
    };

    struct BufRef {
        void* ptr;
        size_t len;
    };

    struct ImageId {
        uint64_t seq;
        ::std::string src;

        ImageId() : seq(0) { }

        ::std::string encode() const;
        bool decode(const BufRef&);

        struct Op {
            void operator() (Graph::Ctx);
        };

        struct Factory {
            Graph::OpFunc operator()() const { return Op(); }
        };

        static bool extract(const BufRef&, ImageId&, bool genSeq = true);
        static bool inject(BufRef&, size_t capacity, const ImageId&);
    };

    struct ImageSave {

        struct Op {
            ::std::string prefix, suffix;
            Op(const ::std::string& _prefix,
               const ::std::string& _suffix)
            : prefix(_prefix), suffix(_suffix) { }
            void operator() (Graph::Ctx);
        };

        struct Factory {
            ::std::string prefix, suffix;
            Factory(const ::std::string& _prefix = "img",
               const ::std::string& _suffix = ".jpg")
            : prefix(_prefix), suffix(_suffix) { }
            Graph::OpFunc operator()() const { return Op(prefix, suffix); }
        };
    };

    struct DetectBox {
        int   category;
        float confidence;
        int   x0, y0, x1, y1;
    };

    struct DetectBoxesPub {
        struct Op {
            MQTTClient *client;
            ::std::string topic;
            Op(MQTTClient *c, const ::std::string& t) : client(c), topic(t) { }
            void operator() (Graph::Ctx);
            Graph::OpFunc operator() () const { return *const_cast<Op*>(this); }
        };
    };
}

#endif
