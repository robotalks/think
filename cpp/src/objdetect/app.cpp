#include <exception>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <boost/network/protocol/http/server.hpp>
#include <boost/network/uri.hpp>

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
DEFINE_string(http_addr, "", "image server listening address");
DEFINE_string(http_port, "2052", "image server listening port");

struct pub_op {
    mqtt::client *client;
    pub_op(mqtt::client* c) : client(c) { }
    void operator() (graph::ctx ctx) {
        const auto& str = ctx.in(0)->as<string>();
        client->publish(FLAGS_mqtt_topic, str);
    }
};

static string index_html(R"(<!DOCTYPE html>
<html>
<head>
<script language="javascript">
function refreshImage() {
    document.getElementById('cam0').src = '/jpeg?ts='+Date.now();
    setTimeout(refreshImage, 30);
}
document.addEventListener('DOMContentLoaded', refreshImage);
</script>
</head>
<body> 
<img id="cam0">
</body>
</html>
)");

static string sz2str(size_t sz) {
    char str[64];
    sprintf(str, "%u", sz);
    return str;
}

class image_handler;
typedef ::boost::network::http::server<image_handler> http_server_t;

class image_handler {
public:
    image_handler() : m_buffer(2*max_buf_size), m_sizes(2), m_current_buf(0) { }

    void set_image(const void *buf, size_t size) {
        int write_buf = 1 - m_current_buf.load();
        if (size > max_buf_size) size = max_buf_size;
        memcpy(&m_buffer[max_buf_size*write_buf], buf, size);
        m_sizes[write_buf] = size;
        atomic_exchange(&m_current_buf, write_buf);
    }

    void operator() (http_server_t::request const& request, http_server_t::connection_ptr conn) {
        VLOG(4) << request.method << " " << request.destination;
        boost::network::uri::uri uri("http://localhost:80"+request.destination);

        static http_server_t::response_header error_headers[] = {
            {"Content-type", "text/plain"},
            {"Connection", "close"},
        };
        if (request.method == "GET") {
            if (uri.path() == "/") {
                handle_index(conn);
                return;
            }
            if (uri.path() == "/jpeg") {
                handle_jpeg(conn);
                return;
            }
            if (uri.path() == "/stream") {
                handle_stream(conn);
                return;
            }
            conn->set_status(http_server_t::connection::not_found);
            conn->set_headers(boost::make_iterator_range(error_headers, 
                error_headers+sizeof(error_headers)/sizeof(error_headers[0])));
            conn->write("Path not supported");
            return;
        }
        conn->set_status(http_server_t::connection::not_supported);
        conn->set_headers(boost::make_iterator_range(error_headers, 
            error_headers+sizeof(error_headers)/sizeof(error_headers[0])));
        conn->write("Method not supported");
    }

    graph::op_func op() {
        return [this](graph::ctx ctx) {
            const buf_ref &buf = ctx.in(0)->as<buf_ref>();
            set_image(buf.ptr, buf.len);
        };
    }

private:
    static constexpr size_t max_buf_size = 0x10000;

    vector<unsigned char> m_buffer;
    vector<size_t> m_sizes;
    atomic_int m_current_buf;

    void handle_index(http_server_t::connection_ptr conn) {
        http_server_t::response_header common_headers[] = {
            {"Content-type", "text/html"},
            {"Content-length", sz2str(index_html.length())},
        };
        conn->set_status(http_server_t::connection::ok);
        conn->set_headers(boost::make_iterator_range(common_headers, 
            common_headers+sizeof(common_headers)/sizeof(common_headers[0])));
        conn->write(index_html);
    }

    void handle_jpeg(http_server_t::connection_ptr conn) {
        int read_buf = m_current_buf.load();
        http_server_t::response_header common_headers[] = {
            {"Content-type", "image/jpeg"},
            {"Content-length", sz2str(m_sizes[read_buf])},
            {"Cache-control", "max-age=0, no-cache"},
        };
        conn->set_status(http_server_t::connection::ok);
        conn->set_headers(boost::make_iterator_range(common_headers, 
            common_headers+sizeof(common_headers)/sizeof(common_headers[0])));
        conn->write(boost::asio::const_buffers_1(&m_buffer[read_buf*max_buf_size], m_sizes[read_buf]), 
            [](boost::system::error_code const&) {});
    }

    void handle_stream(http_server_t::connection_ptr conn) {
        conn->set_status(http_server_t::connection::not_supported);
        conn->write("Not implemented");
    }
};

class app {
public:
    app() {
        mqtt::initialize();

        http_server_t::options options(m_imghandler);
        options.reuse_address(true)
            .thread_pool(make_shared<boost::network::utils::thread_pool>())
            .port(FLAGS_http_port);
        if (!FLAGS_http_addr.empty()) {
            options.address(FLAGS_http_addr);
        }
        m_httpsrv.reset(new http_server_t(options));

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
            g->def_vars({"input", "id", "size", "pixels", "objects", "result"});
            g->add_op("imgid", {"input"}, {"id"}, op::image_id());
            g->add_op("decode", {"input"}, {"pixels", "size"}, op::decode_image());
            g->add_op("detect", {"pixels"}, {"objects"}, movidius::op::ssd_mobilenet(model.get()));
            g->add_op("json", {"size", "id", "objects"}, {"result"}, op::detect_boxes_json());
            g->add_op("publish", {"result"}, {}, pub_op(m_mqtt_client.get()));
            g->add_op("imagesrv", {"input"}, {}, m_imghandler.op());
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
        thread httpsrv_thread([this] { m_httpsrv->run(); });
        udp.run(&m_dispatcher);
        httpsrv_thread.join();
    }

private:
    unique_ptr<mqtt::client> m_mqtt_client;
    vector<unique_ptr<movidius::compute_stick>> m_ncs;
    vector<unique_ptr<movidius::compute_stick::graph>> m_models;
    vector<unique_ptr<graph>> m_graphs;
    graph_dispatcher m_dispatcher;
    image_handler m_imghandler;
    unique_ptr<http_server_t> m_httpsrv;
};

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    app app;
    app.run();
    return 0;
}
