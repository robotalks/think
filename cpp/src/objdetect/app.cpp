#include <exception>
#include <iostream>
#include <thread>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "dp/graph.h"
#include "dp/operators.h"
#include "dp/ingress/udp.h"
#include "dp/ingress/videocap.h"
#include "mqtt/mqtt.h"
#include "movidius/ncs.h"
#include "movidius/ssd_mobilenet.h"

using namespace std;
using namespace dp;

DEFINE_int32(port, in::udp::default_port, "listening port");
DEFINE_string(mqtt_host, "127.0.0.1", "MQTT server address");
DEFINE_int32(mqtt_port, mqtt::client::default_port, "MQTT server port");
DEFINE_string(mqtt_client_id, "", "MQTT client ID");
DEFINE_string(mqtt_topic, "", "MQTT topic");
DEFINE_string(model, "graph", "Model name");

struct pub_op {
    mqtt::client *client;
    pub_op(mqtt::client* c) : client(c) { }
    void operator() (graph::ctx ctx) {
        const auto& sz = ctx.in(0)->as<image_size>();
        const auto& id = ctx.in(1)->as<image_id>();
        const auto& boxes = ctx.in(2)->as<vector<detect_box>>();
        char cstr[1024];
        sprintf(cstr, "{\"src\":\"%s\",\"seq\":%lu,\"size\":[%d,%d],\"boxes\":[",
            id.src.c_str(), id.seq, sz.w, sz.h);
        string str(cstr);
        for (auto& b : boxes) {
            sprintf(cstr, "{\"class\":%d,\"score\":%f,\"rc\":[%d,%d,%d,%d]},",
                b.category, b.confidence, b.x0, b.y0, b.x1, b.y1);
            str += cstr;
        }
        str = str.substr(0, str.length()-1) + "]}";
        client->publish(FLAGS_mqtt_topic, str);
    }
};

class app {
public:
    app() {
        mqtt::initialize();

        auto names = movidius::compute_stick::devices();
        if (names.empty()) throw runtime_error("no Movidius NCS found");

        LOG(INFO) << "MQTT Connect " << FLAGS_mqtt_host << ":" << FLAGS_mqtt_port;
        m_mqtt_client.reset(new mqtt::client(FLAGS_mqtt_client_id));
        m_mqtt_client->connect(FLAGS_mqtt_host, (unsigned short)FLAGS_mqtt_port).wait();

        for (auto& name : names) {
            LOG(INFO) << "Loading " << name << " with model " << FLAGS_model;
            unique_ptr<movidius::compute_stick> stick(new movidius::compute_stick(name));
            unique_ptr<movidius::compute_stick::graph> model(stick->alloc_graph_from_file(FLAGS_model));
            unique_ptr<graph> g(new graph(name));
            g->def_vars({"input", "id", "size", "pixels", "objects"});
            g->add_op("imgid", {"input"}, {"id"}, op::image_id());
            g->add_op("decode", {"input"}, {"pixels", "size"}, op::decode_image());
            g->add_op("detect", {"pixels"}, {"objects"}, movidius::op::ssd_mobilenet(model.get()));
            g->add_op("publish", {"size", "id", "objects"}, {}, pub_op(m_mqtt_client.get()));
            m_dispatcher.add_graph(g.get());
            m_ncs.push_back(move(stick));
            m_models.push_back(move(model));
            m_graphs.push_back(move(g));
        }
    }

    ~app() {
        mqtt::cleanup();
    }

    void run() {
        LOG(INFO) << "Run!";
        in::udp udp((uint16_t)FLAGS_port);
        udp.run(&m_dispatcher);
    }

private:
    unique_ptr<mqtt::client> m_mqtt_client;
    vector<unique_ptr<movidius::compute_stick>> m_ncs;
    vector<unique_ptr<movidius::compute_stick::graph>> m_models;
    vector<unique_ptr<graph>> m_graphs;
    graph_dispatcher m_dispatcher;
};

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    app app;
    app.run();
    return 0;
}
