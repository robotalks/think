#ifndef __DP_UTIL_QUEUE_H
#define __DP_UTIL_QUEUE_H

#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>

namespace dp {
    template<typename T>
    class queue {
    public:
        queue() { }

        T get() {
            ::std::unique_lock<::std::mutex> lock(m_mutex);
            while (m_queue.empty()) m_cv.wait(lock);
            T v = m_queue.front();
            m_queue.pop_front();
            return v;
        }

        void put(T v) {
            ::std::unique_lock<::std::mutex> lock(m_mutex);
            m_queue.push_back(v);
            m_cv.notify_one();
        }

        bool empty() {
            ::std::unique_lock<::std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

    private:
        ::std::list<T> m_queue;
        ::std::mutex m_mutex;
        ::std::condition_variable m_cv;
    };

    template<typename T>
    class single_consumer_queue {
    public:
        single_consumer_queue(size_t max_num)
        : m_queue(max_num), m_get(0), m_put(0) { }

        T get() {
            if (m_get >= m_queue.size()) return nullptr;
            if (m_get >= m_put) {
                ::std::unique_lock<::std::mutex> lock(m_mutex);
                while (m_get >= m_put) m_cv.wait(lock);
            }
            return m_queue[m_get++];
        }

        void put(T v) {
            if (m_put < m_queue.size()) {
                ::std::unique_lock<::std::mutex> lock(m_mutex);
                m_queue[m_put++] = v;
                m_cv.notify_one();
            }
        }

    private:
        ::std::vector<T> m_queue;
        size_t m_get;
        volatile size_t m_put;
        ::std::mutex m_mutex;
        ::std::condition_variable m_cv;
    };
}

#endif
