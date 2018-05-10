#ifndef __VP_UTIL_POOL_H
#define __VP_UTIL_POOL_H

#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>

namespace vp {
    class IndexPool {
    public:
        static constexpr size_t none = (size_t)(-1);

        IndexPool(size_t size = 0);

        void resize(size_t size);

        size_t get(bool nowait = false);
        void put(size_t);

    private:
        ::std::vector<size_t> m_ring;
        volatile size_t m_get;
        volatile size_t m_put;
        ::std::mutex m_put_mutex;
        ::std::condition_variable m_put_cv;
    };
}

#endif
