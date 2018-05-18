#ifndef __MOVIDIUS_NCS_H
#define __MOVIDIUS_NCS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace movidius {
    class compute_stick {
    public:
        static ::std::vector<::std::string> devices();

        compute_stick(const ::std::string& name);
        compute_stick(const compute_stick&) = delete;
        compute_stick(compute_stick&&);
    
        virtual ~compute_stick();

        compute_stick& operator = (const compute_stick&) = delete;
        compute_stick& operator = (compute_stick&&);
    
        const ::std::string& name() const { return m_name; }

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
        ::std::string m_name;
        void* m_handle;
    };

    class device_pool {
    public:
        device_pool();

        size_t populate();

        compute_stick* alloc(const ::std::string& name = ::std::string());

        void release(compute_stick*);

    private:
        struct device {
            ::std::unique_ptr<compute_stick> stick;
            int refs;

            device(const ::std::string& name);
            device(const device&) = delete;
            device(device&& d) : stick(::std::move(d.stick)), refs(d.refs) { }
        };

        ::std::vector<device> m_devices;
        ::std::unordered_map<::std::string, size_t> m_names;
    };

    float half2float(uint16_t);
    uint16_t float2half(float);
}

#endif
