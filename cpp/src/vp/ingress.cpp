#include "vp/ingress.h"

namespace vp {
    using namespace std;

    Ingress::Ingress()
    : m_input_var("input"), m_running(true) {

    }

    Ingress::Ingress(const string& input_var)
    : m_input_var(input_var), m_running(true) {

    }

    Ingress::~Ingress() {

    }

    bool Ingress::recv(Dispatcher *disp, bool nowait) {
        Session session;
        if (prepareSession(session, nowait)) {
            return disp->dispatch(session, nowait);
        }
        return false;
    }

    void Ingress::run(Dispatcher *disp) {
        while (running()) {
            recv(disp);
        }
    }

    IngressRunner::IngressRunner() {

    }

    IngressRunner::~IngressRunner() {

    }

    void IngressRunner::addIngress(Ingress *ingress) {
        m_ingresses.push_back(ingress);
    }

    void IngressRunner::start(Dispatcher *disp) {
        for (auto ingress : m_ingresses) {
            unique_ptr<thread> thread_ptr(new thread([ingress, disp] {
                ingress->run(disp);
            }));
            m_threads.push_back(move(thread_ptr));
        }
    }

    void IngressRunner::stop() {
        for (auto ingress : m_ingresses) {
            ingress->stop();
        }
        for (auto& thread_ptr : m_threads) {
            thread_ptr->join();
        }
        m_threads.clear();
    }
}
