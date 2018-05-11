#ifndef __DP_SCHEDULER_H
#define __DP_SCHEDULER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "dp/graph.h"
#include "dp/util/pool.h"

namespace dp {
    using graph_handle_func = ::std::function<void(graph*)>;

    struct session {
        graph_handle_func initializer;
        graph_handle_func finalizer;
        session() { }
        session(const graph_handle_func& init) : initializer(init) { }
        session(const graph_handle_func& init, const graph_handle_func& fini) : initializer(init), finalizer(fini) { }
    };

    struct dispatcher {
        virtual ~dispatcher() { }
        virtual bool dispatch(const session&, bool nowait = false) = 0;
    };

    class graph_dispatcher : public dispatcher {
    public:
        graph_dispatcher();
        virtual ~graph_dispatcher();

        size_t add_graph(graph* g);

        bool dispatch(const session&, bool nowait = false);

    private:
        struct slot {
            graph *g;
            size_t index;
            volatile ::std::thread *worker;
            ::std::mutex worker_mutex;

            slot(graph *_g, size_t idx) : g(_g), index(idx), worker(nullptr) { }
            ~slot() { }

            void run(graph_dispatcher*, const session&);
        };

        ::std::vector<::std::unique_ptr<slot>> m_slots;
        index_pool m_pool;

        friend class slot;
    };
}

#endif
