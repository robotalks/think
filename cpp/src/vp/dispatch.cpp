#include "vp/graph.h"
#include "vp/dispatch.h"

namespace vp {
    using namespace std;

    Pipeline::Pipeline() {
    }

    Pipeline::~Pipeline() {
    }

    size_t Pipeline::addGraph(Graph* g) {
        size_t idx = m_slots.size();
        m_slots.push_back(move(unique_ptr<Slot>(new Slot(g, idx))));
        m_pool.resize(m_slots.size());
        return idx;
    }

    bool Pipeline::dispatch(const Session& s, bool nowait) {
        size_t idx = m_pool.get(nowait);
        if (idx == IndexPool::none) return false;
        m_slots[idx]->run(this, s);
        return true;
    }

    void Pipeline::Slot::run(Pipeline *p, const Session& s) {
        unique_lock<mutex> lock(worker_mutex);
        worker = new thread([this, p, s] () {
            graph->reset();
            if (s.initializer) s.initializer(graph);
            graph->exec();
            if (s.finalizer) s.finalizer(graph);
            graph->reset();

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
