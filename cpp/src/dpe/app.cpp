#include <exception>
#include <iostream>
#include <thread>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "dp/graph.h"
#include "dp/graph_def.h"
#include "dp/operators.h"
#include "dp/ingress/udp.h"
#include "dp/ingress/videocap.h"
#include "mqtt/operators.h"
#include "movidius/operators.h"

using namespace std;
using namespace dp;

class app {
public:
    app(int argc, char* argv[]) {
        mqtt::initialize();

        if (argc > 1) {
            m_def.parse_file(argv[1]);
        } else {
            m_def.parse(cin);
        }
    }

    ~app() {
        mqtt::cleanup();
    }

    void run() {
        exec_env env;
        m_def.build_env(env, {}).run();
    }

private:
    graph_def m_def;
};

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    dp::in::udp::register_factory();
    dp::in::video_capture::register_factory();
    dp::op::register_factories();
    mqtt::op::register_factories();
    movidius::op::register_factories();

    app(argc, argv).run();
    return 0;
}
