#include <cstring>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <glog/logging.h>
#include <mvnc.h>

#include "movidius/ncs.h"

namespace movidius {
    using namespace std;

    static void check_mvnc_status(mvncStatus s) {
        char str[128];
        sprintf(str, "mvnc: %d", s);

        switch (s) {
        case MVNC_INVALID_PARAMETERS: throw invalid_argument(str);
        case MVNC_OK: return;
        default: throw runtime_error(str);
        }
    }

    vector<string> compute_stick::devices() {
        vector<string> names;
        char name[128];
        int index = 0;
        while (mvncGetDeviceName(index, name, sizeof(name)) != MVNC_DEVICE_NOT_FOUND) {
            names.push_back(string(name));
            index ++;
        }
        return names;
    }

    compute_stick::compute_stick(const string& name)
    : m_name(name), m_handle(nullptr) {
        mvncStatus r = mvncOpenDevice(name.c_str(), &m_handle);
        check_mvnc_status(r);
    }

    compute_stick::compute_stick(compute_stick&& s) 
    : m_name(move(s.m_name)), m_handle(s.m_handle) {
        s.m_handle = nullptr;
    }

    compute_stick::~compute_stick() {
        if (m_handle != nullptr)
            mvncCloseDevice(m_handle);
    }

    compute_stick& compute_stick::operator = (compute_stick&& s) {
        if (m_handle != nullptr)
            mvncCloseDevice(m_handle);
        m_name = move(s.m_name);
        m_handle = s.m_handle;
        s.m_handle = nullptr;
        return *this;
    }

    compute_stick::graph* compute_stick::alloc_graph(const void *graph_data, size_t len) {
        void* handle = nullptr;
        mvncStatus r = mvncAllocateGraph(m_handle, &handle, graph_data, len);
        check_mvnc_status(r);
        return new graph(handle);
    }

    compute_stick::graph* compute_stick::alloc_graph_from_file(const string& fn) {
        ifstream f;
        f.exceptions(ios::failbit|ios::badbit);
        f.open(fn, ios::in|ios::binary);
        f.seekg(0, f.end);
        auto len = f.tellg();
        unique_ptr<char> buf(new char[len]);
        if (buf == nullptr) throw runtime_error("not enough memory for graph file " + fn);
        f.seekg(0, f.beg);
        f.read(buf.get(), len);
        f.close();
        return alloc_graph(buf.get(), len);
    }

    compute_stick::graph::graph(void* handle)
    : m_handle(handle) {

    }

    compute_stick::graph::~graph() {
        check_mvnc_status(mvncDeallocateGraph(m_handle));
    }

    void compute_stick::graph::exec(const void* data, size_t len,
        function<void(const void*, size_t)> done) {
        check_mvnc_status(mvncLoadTensor(m_handle, data, len, nullptr));
        void *output = nullptr, *opaque = nullptr;
        unsigned int outlen = 0;
        check_mvnc_status(mvncGetResult(m_handle, &output, &outlen, &opaque));
        done(output, outlen);
    }

    device_pool::device_pool() {
    }

    size_t device_pool::populate() {
        auto names = compute_stick::devices();
        for (auto& name : names) {
            device d(name);
            m_names.insert(make_pair(name, m_devices.size()));
            m_devices.push_back(move(d));
        }
        return m_devices.size();
    }

    compute_stick* device_pool::alloc(const string &name) {
        if (name.empty()) {
            for (auto& d : m_devices) {
                if (d.refs == 0) {
                    d.refs ++;
                    return d.stick.get();
                }
            }
            return nullptr;
        }
        auto it = m_names.find(name);
        if (it == m_names.end()) return nullptr;
        device& d = m_devices[it->second];
        if (d.refs == 0) {
            d.refs ++;
            return d.stick.get();
        }
        return nullptr;
    }

    void device_pool::release(compute_stick* stick) {
        for (auto& d : m_devices) {
            if (d.stick.get() == stick) {
                d.refs --;
                break;
            }
        }
    }

    device_pool::device::device(const string& name)
    : refs(0) {
        stick.reset(new compute_stick(name));
    }
}
