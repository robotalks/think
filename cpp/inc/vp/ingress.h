#ifndef __VP_INGRESS_H
#define __VP_INGRESS_H

#include <string>
#include "vp/graph.h"
#include "vp/dispatch.h"

namespace vp {
    class Ingress {
    public:
        Ingress();
        Ingress(const ::std::string& input_var);
        virtual ~Ingress();

        const ::std::string& inputVar() const { return m_input_var; }
        void inputVar(const ::std::string& name) { m_input_var = name; }

        virtual bool recv(Dispatcher*, bool nowait = false);
        virtual void run(Dispatcher*);

        virtual void stop() { m_running = false; }

    protected:
        ::std::string m_input_var;
        volatile bool m_running;

        virtual bool prepareSession(Session&, bool nowait) { return false; }
        virtual bool running() const { return m_running; }
    };

    class IngressRunner {
    public:
        IngressRunner();
        virtual ~IngressRunner();

        void addIngress(Ingress*);

        void start(Dispatcher*);
        void stop();

    private:
        ::std::vector<Ingress*> m_ingresses;
        ::std::vector<::std::unique_ptr<::std::thread>> m_threads;
    };
}

#endif
