#ifndef __DP_INGRESS_H
#define __DP_INGRESS_H

#include <string>
#include "dp/graph.h"
#include "dp/dispatch.h"

namespace dp {
    class ingress {
    public:
        ingress();
        ingress(const ::std::string& input_var);
        virtual ~ingress();

        const ::std::string& input_var() const { return m_input_var; }
        void input_var(const ::std::string& name) { m_input_var = name; }

        virtual bool recv(dispatcher*, bool nowait = false);
        virtual void run(dispatcher*);

        virtual void stop() { m_running = false; }

    protected:
        ::std::string m_input_var;
        volatile bool m_running;

        virtual bool prepare_session(session&, bool nowait) { return false; }
        virtual bool running() const { return m_running; }

    public:
        class runner {
        public:
            runner();
            virtual ~runner();

            void add(ingress*);

            void start(dispatcher*);
            void stop();
            void join();

        private:
            ::std::vector<ingress*> m_ingresses;
            ::std::vector<::std::unique_ptr<::std::thread>> m_threads;
        };
    };

    struct exec_env {
        graph_dispatcher dispatcher;
        ingress::runner runner;
        ::std::list<::std::unique_ptr<graph>> graphs;
        ::std::list<::std::unique_ptr<ingress>> ingresses;

        void build();

        void start() { runner.start(&dispatcher); }
        void stop() { runner.stop(); }

        void run() {
            start();
            runner.join();
        }
    };
}

#endif
