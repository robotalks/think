#ifndef __VP_SCHEDULER_H
#define __VP_SCHEDULER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "vp/graph.h"
#include "vp/util/pool.h"

namespace vp {
    using GraphHandler = ::std::function<void(Graph*)>;

    struct Session {
        GraphHandler initializer;
        GraphHandler finalizer;
        Session() { }
        Session(const GraphHandler& init) : initializer(init) { }
        Session(const GraphHandler& init, const GraphHandler& fini) : initializer(init), finalizer(fini) { }
    };

    struct Dispatcher {
        virtual ~Dispatcher() { }
        virtual bool dispatch(const Session&, bool nowait = false) = 0;
    };

    class Pipeline : public Dispatcher {
    public:
        Pipeline();
        virtual ~Pipeline();

        size_t addGraph(Graph* g);

        bool dispatch(const Session&, bool nowait = false);

    private:
        struct Slot {
            Graph *graph;
            size_t index;
            volatile ::std::thread *worker;
            ::std::mutex worker_mutex;

            Slot(Graph *g, size_t idx) : graph(g), index(idx), worker(nullptr) { }
            ~Slot() { }

            void run(Pipeline*, const Session&);
        };

        ::std::vector<::std::unique_ptr<Slot>> m_slots;
        IndexPool m_pool;

        friend class Slot;
    };
}

#endif
