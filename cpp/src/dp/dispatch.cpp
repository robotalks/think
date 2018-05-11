#include "dp/graph.h"
#include "dp/dispatch.h"

namespace dp {
    using namespace std;

    graph_dispatcher::graph_dispatcher() {
    }

    graph_dispatcher::~graph_dispatcher() {
    }

    size_t graph_dispatcher::add_graph(graph* g) {
        size_t idx = m_slots.size();
        m_slots.push_back(move(unique_ptr<slot>(new slot(g, idx))));
        m_pool.resize(m_slots.size());
        return idx;
    }

    bool graph_dispatcher::dispatch(const session& s, bool nowait) {
        size_t idx = m_pool.get(nowait);
        if (idx == index_pool::none) return false;
        m_slots[idx]->run(this, s);
        return true;
    }

    void graph_dispatcher::slot::run(graph_dispatcher *p, const session& s) {
        unique_lock<mutex> lock(worker_mutex);
        worker = new thread([this, p, s] () {
            g->reset();
            if (s.initializer) s.initializer(g);
            g->exec();
            if (s.finalizer) s.finalizer(g);
            g->reset();

            auto t = const_cast<thread*>(worker);
            {
                unique_lock<mutex> lock_slot(worker_mutex);
                worker = nullptr;
                p->m_pool.put(index);
            }
            t->detach();
            delete t;
        });
    }
}
