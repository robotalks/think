#ifndef __MOVIDIUS_NCS_H
#define __MOVIDIUS_NCS_H

#include <string>
#include <vector>
#include <functional>

namespace movidius {
    class compute_stick {
    public:
        static ::std::vector<::std::string> devices();

        compute_stick(const ::std::string& name);
        virtual ~compute_stick();

        class graph {
        public:
            virtual ~graph();

            void exec(const void*, size_t, ::std::function<void(const void*, size_t)>);

        private:
            graph(void* handle);
            void* m_handle;

            friend class compute_stick;
        };

        graph* alloc_graph(const void* graph, size_t len);
        graph* alloc_graph_from_file(const ::std::string& fn);

    private:
        void* m_handle;
    };

    float half2float(uint16_t);
    uint16_t float2half(float);
}

#endif
