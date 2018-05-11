#include "dp/util/pool.h"

namespace dp {
    using namespace std;

    index_pool::index_pool(size_t size) {
        resize(size);
    }

    void index_pool::resize(size_t size) {
        m_ring.resize(size+1);
        for (size_t i = 0; i < size; i ++) {
            m_ring[i+1] = i;
        }
        m_get = 1;
        m_put = size > 0 ? 0 : 1;
    }

    size_t index_pool::get(bool nowait) {
        if (m_get == m_put) {
            if (nowait) return none;
            unique_lock<mutex> lock(m_put_mutex);
            while (m_get == m_put)
                m_put_cv.wait(lock);
        }
        auto index = m_ring[m_get++];
        if (m_get >= m_ring.size()) m_get = 0;
        return index;
    }

    void index_pool::put(size_t index) {
        unique_lock<mutex> lock_return(m_put_mutex);
        m_ring[m_put++] = index;
        if (m_put >= m_ring.size()) m_put = 0;
        m_put_cv.notify_one();
    }
}
