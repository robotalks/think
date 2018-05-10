#include <exception>
#include <iostream>
#include <thread>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "vp/graph.h"
#include "vp/operators.h"
#include "vp/cv/operators.h"
#include "vp/cv/videocap.h"
#include "vp/ingress/udp.h"
#include "vp/mqtt/mqtt.h"

#include "movidius/ncs.h"

using namespace std;
using namespace vp;
using namespace movidius;

DEFINE_int32(port, UDPIngress::PORT, "listening port");
DEFINE_string(mqtt_host, "127.0.0.1", "MQTT server address");
DEFINE_int32(mqtt_port, MQTTClient::PORT, "MQTT server port");
DEFINE_string(mqtt_client_id, "", "MQTT client ID");
DEFINE_string(mqtt_topic, "", "MQTT topic");
DEFINE_string(model, "graph", "Model name");

class App {
public:
    App() {
        MQTT::initialize();

        auto names = NeuralComputeStick::devices();
        if (names.empty()) throw runtime_error("no Movidius NCS found");

        LOG(INFO) << "MQTT Connect " << FLAGS_mqtt_host << ":" << FLAGS_mqtt_port;
        m_mqtt_client.reset(new MQTTClient(FLAGS_mqtt_client_id));
        m_mqtt_client->connect(FLAGS_mqtt_host, (unsigned short)FLAGS_mqtt_port).wait();

        for (auto& name : names) {
            LOG(INFO) << "Loading " << name << " with model " << FLAGS_model;
            unique_ptr<NeuralComputeStick> ncs(new NeuralComputeStick(name));
            unique_ptr<NeuralComputeStick::Graph> model(ncs->loadGraphFile(FLAGS_model));
            unique_ptr<Graph> g(new Graph(name));
            g->defVars({"input", "id", "pixels", "objects"});
            g->addOp("imgid", {"input"}, {"id"}, ImageId::Op());
            g->addOp("decode", {"input"}, {"pixels"}, CvImageDecode::Op());
            g->addOp("detect", {"pixels"}, {"objects"}, SSDMobileNet::Op(model.get()));
            g->addOp("publish", {"pixels", "id", "objects"}, {}, DetectBoxesPub::Op(m_mqtt_client.get(), FLAGS_mqtt_topic));
            m_pipeline.addGraph(g.get());
            m_ncs.push_back(move(ncs));
            m_models.push_back(move(model));
            m_graphs.push_back(move(g));
        }
    }

    ~App() {
        MQTT::cleanup();
    }

    void run() {
        LOG(INFO) << "Run!";
        UDPIngress udp((uint16_t)FLAGS_port);
        udp.run(&m_pipeline);
    }

private:
    unique_ptr<MQTTClient> m_mqtt_client;
    vector<unique_ptr<NeuralComputeStick>> m_ncs;
    vector<unique_ptr<NeuralComputeStick::Graph>> m_models;
    vector<unique_ptr<Graph>> m_graphs;
    Pipeline m_pipeline;
};

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    App app;
    app.run();
    return 0;
}
