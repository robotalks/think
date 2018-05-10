#ifndef __MOVIDIUS_NCS_H
#define __MOVIDIUS_NCS_H

#include <string>
#include <vector>
#include <functional>
#include <opencv2/core/core.hpp>

#include "vp/graph.h"
#include "vp/operators.h"

namespace movidius {
    class NeuralComputeStick {
    public:
        static ::std::vector<::std::string> devices();

        NeuralComputeStick(const ::std::string& name);
        virtual ~NeuralComputeStick();

        class Graph {
        public:
            virtual ~Graph();

            void exec(const void*, size_t, ::std::function<void(const void*, size_t)>);

            ::vp::Graph::OpFunc op();
            ::vp::Graph::OpFactory opFactory();

        private:
            Graph(void* handle);
            void* m_handle;

            friend class NeuralComputeStick;
        };

        Graph* allocGraph(const void* graph, size_t len);
        Graph* loadGraphFile(const ::std::string& fn);

    private:
        void* m_handle;
    };

    float half2float(uint16_t);
    uint16_t float2half(float);

    struct CropFp16Op {
        int cx, cy;
        CropFp16Op(int _cx, int _cy) : cx(_cx), cy(_cy) { }
        ::vp::Graph::OpFunc operator()() const;

        static ::cv::Mat cropFp16(const ::cv::Mat&, int cx, int cy);
    };

    struct SSDMobileNet {
        static constexpr int ImageSz = 300;

        struct Factory {
            NeuralComputeStick::Graph *graph;
            Factory(NeuralComputeStick::Graph *g) : graph(g) { }
            ::vp::Graph::OpFunc operator()() const;
        };

        struct Op {
            NeuralComputeStick::Graph *graph;
            Op(NeuralComputeStick::Graph *g) : graph(g) { }
            void operator() (::vp::Graph::Ctx);
        };

        static ::std::vector<::vp::DetectBox> parseResult(
            const void* out, size_t len, int origCX, int origCY);
    };

    inline ::vp::Graph::OpFunc SSDMobileNet::Factory::operator()() const {
        return SSDMobileNet::Op(graph);
    }
}

#endif
