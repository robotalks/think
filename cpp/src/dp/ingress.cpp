#include "dp/ingress.h"

namespace dp {
    using namespace std;

    ingress::ingress()
    : m_input_var("input"), m_running(true) {

    }

    ingress::ingress(const string& input_var)
    : m_input_var(input_var), m_running(true) {

    }

    ingress::~ingress() {

    }

    bool ingress::recv(dispatcher *disp, bool nowait) {
        session session;
        if (prepare_session(session, nowait)) {
            return disp->dispatch(session, nowait);
        }
        return false;
    }

    void ingress::run(dispatcher *disp) {
        while (running()) {
            recv(disp);
        }
    }

    ingress::runner::runner() {

    }

    ingress::runner::~runner() {

    }

    void ingress::runner::add(ingress *ingress) {
        m_ingresses.push_back(ingress);
    }

    void ingress::runner::start(dispatcher *disp) {
        for (auto ingress : m_ingresses) {
            unique_ptr<thread> thread_ptr(new thread([ingress, disp] {
                ingress->run(disp);
            }));
            m_threads.push_back(move(thread_ptr));
        }
    }

    void ingress::runner::stop() {
        for (auto ingress : m_ingresses) {
            ingress->stop();
        }
        for (auto& thread_ptr : m_threads) {
            thread_ptr->join();
        }
        m_threads.clear();
    }
}
