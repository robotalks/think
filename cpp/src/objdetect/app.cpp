#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <exception>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <boost/network/protocol/http/server.hpp>
#include <boost/network/uri.hpp>

#include "dp/graph.h"
#include "dp/operators.h"
#include "dp/ingress/udp.h"
#include "dp/ingress/videocap.h"
#include "dp/util/error.h"
#include "mqtt/mqtt.h"
#include "movidius/ncs.h"
#include "movidius/ssd_mobilenet.h"

using namespace std;
using namespace dp;

static constexpr int http_port = 2052;
static constexpr int cast_port = 2053;

DEFINE_int32(port, in::udp::default_port, "listening port");
DEFINE_string(mqtt_host, "127.0.0.1", "MQTT server address");
DEFINE_int32(mqtt_port, mqtt::client::default_port, "MQTT server port");
DEFINE_string(mqtt_client_id, "", "MQTT client ID");
DEFINE_string(mqtt_topic, "", "MQTT topic");
DEFINE_string(model, "graph", "Model name");
DEFINE_string(http_addr, "", "image server listening address");
DEFINE_int32(http_port, http_port, "image server listening port");
DEFINE_int32(stream_port, cast_port, "TCP streaming port");

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

struct image_buffer {
    vector<unsigned char> data;
    size_t size;
    image_id id;

    static constexpr size_t max_buf_size = 0x10000;

    image_buffer() : data(max_buf_size), size(0) { }

    const unsigned char* ptr() const {
        return &data[0];
    }

    void update(const void *buf, size_t _sz, const image_id& _id) {
        if (_sz > max_buf_size) _sz = max_buf_size;
        memcpy(&data[0], buf, _sz);
        size = _sz;
        id = _id;
    }
};

class image_handler;
typedef ::boost::network::http::server<image_handler> http_server_t;

class image_handler {
public:
    image_handler() : m_images(2), m_current_buf(0) { }

    void set_image(const void *buf, size_t size, const image_id& id) {
        int write_buf = 1 - m_current_buf.load();
        m_images[write_buf].update(buf, size, id);
        atomic_exchange(&m_current_buf, write_buf);
        m_set_image_cv.notify_all();
    }

    const image_buffer* wait() const {
        unique_lock<mutex> lock(m_set_image_mux);
        m_set_image_cv.wait(lock);
        return &m_images[m_current_buf.load()];
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
            const auto& id = ctx.in(1)->as<image_id>();
            set_image(buf.ptr, buf.len, id);
        };
    }

private:
    vector<image_buffer> m_images;
    atomic_int m_current_buf;

    mutable mutex m_set_image_mux;
    mutable condition_variable m_set_image_cv;

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
        const image_buffer& buf = m_images[m_current_buf.load()];
        http_server_t::response_header common_headers[] = {
            {"Content-type", "image/jpeg"},
            {"Content-length", sz2str(buf.size)},
            {"Cache-control", "max-age=0, no-cache"},
            {"X-ImageId-Seq", sz2str(buf.id.seq)},
            {"X-ImageId-Src", buf.id.src},
        };
        conn->set_status(http_server_t::connection::ok);
        conn->set_headers(boost::make_iterator_range(common_headers, 
            common_headers+sizeof(common_headers)/sizeof(common_headers[0])));
        conn->write(boost::asio::const_buffers_1(buf.ptr(), buf.size), 
            [](boost::system::error_code const&) {});
    }

    void handle_stream(http_server_t::connection_ptr conn) {
        conn->set_status(http_server_t::connection::not_supported);
        conn->write("Not implemented");
    }
};

class socket_listener {
public:
    socket_listener(int type, int port) : m_socket(-1) {
        try {
            m_socket = socket(PF_INET, type, 0);
            if (m_socket < 0) {
                throw runtime_error(errmsg("socket"));
            }
            long en = 1;
            if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en)) < 0) {
                throw runtime_error(errmsg("setsockopt"));
            }

            sockaddr_in sa;
            memset(&sa, 0, sizeof(sa));
            sa.sin_family = PF_INET;
            sa.sin_port = htons((u_short)port);
            sa.sin_addr.s_addr = INADDR_ANY;
            if (bind(m_socket, (sockaddr*)&sa, sizeof(sa)) < 0) {
                throw runtime_error(errmsg("socket bind"));
            }

            if (type != SOCK_STREAM) return;

            if (listen(m_socket, SOMAXCONN) < 0) {
                throw runtime_error(errmsg("listen"));
            }
        } catch (runtime_error&) {
            if (m_socket >= 0) {
                close(m_socket);
            }
            throw;
        }
    }

    virtual ~socket_listener() {
        if (m_socket >= 0) {
            close(m_socket);
        }
    }

    int socket_fd() const { return m_socket; }

private:
    int m_socket;
};

class streamer : public socket_listener {
public:
    streamer(int port = cast_port)
    : socket_listener(SOCK_STREAM, port) {
    }

    void run(const image_handler* images) {
        while (true) {
            sockaddr_in sa;
            socklen_t salen = sizeof(sa);
            int conn = accept(socket_fd(), (sockaddr*)&sa, &salen);
            if (conn < 0) {
                LOG(ERROR) << errmsg("accept");
                break;
            }
            thread client([this, conn, images] { stream_to(conn, images); });
            client.detach();
        }
    }

private:
    void stream_to(int conn, const image_handler* images) {
        const image_buffer* buf;
        while ((buf = images->wait()) != nullptr) {
            uint16_t sz = (uint16_t)buf->size;
            int r = send(conn, &sz, sizeof(sz), MSG_NOSIGNAL);
            if (r > 0) {
                r = send(conn, buf->ptr(), buf->size, MSG_NOSIGNAL);
            }
            if (r < 0) {
                LOG(ERROR) << errmsg("send");
                break;
            }
        }
        close(conn);
    }
};

class udp_caster : public socket_listener {
public:
    udp_caster(int port = cast_port)
    : socket_listener(SOCK_DGRAM, port) {
    }

    void run(const image_handler* images) {
        thread listener_thread([this] { run_listener(); });
        run_caster(images);
        listener_thread.join();
    }

private:
    struct subscriber {
        sockaddr_in addr;
        int failures;

        subscriber(const sockaddr_in& sa) : failures(0) {
            memcpy(&addr, &sa, sizeof(addr));
        }
    
        bool same(const sockaddr_in& sa) const {
            return addr.sin_port == sa.sin_port && addr.sin_addr.s_addr == sa.sin_addr.s_addr;
        }

        string str() const {
            string s(inet_ntoa(addr.sin_addr));
            s += ":" + sz2str(ntohs(addr.sin_port));
            return s;
        }

        static function<bool(const subscriber&)> if_same(sockaddr_in sa) {
            return [sa] (const subscriber& sub) -> bool {
                return sub.same(sa);
            };
        }
    };

    list<subscriber> m_subscribers;
    mutex m_lock;

    static constexpr int max_failures = 5;

    void run_listener() {
        char buf[4];
        while (true) {
            sockaddr_in sa;
            socklen_t salen = sizeof(sa);            
            int r = recvfrom(socket_fd(), buf, sizeof(buf), 0, (sockaddr*)&sa, &salen);
            if (r < 0) {
                LOG(ERROR) << errmsg("recvfrom");
                break;
            }
            if (r == 0) continue;

            switch (buf[0]) {
            case '+':
                add_subscriber(sa);
                break;
            case '-':
                del_subscriber(sa);
                break;
            }
        }
    }

    void run_caster(const image_handler* images) {
        const image_buffer* buf;
        while ((buf = images->wait()) != nullptr) {
            uint16_t sz = (uint16_t)buf->size;
            cast(buf);
        }
    }

    void cast(const image_buffer* buf) {
        unique_lock<mutex> lock(m_lock);
        list<subscriber>::iterator it = m_subscribers.begin();
        while (it != m_subscribers.end()) {
            auto cur = it;
            it ++;
            int r = sendto(socket_fd(), buf->ptr(), buf->size, MSG_NOSIGNAL, 
                (sockaddr*)&cur->addr, sizeof(cur->addr));
            if (r < 0) {
                LOG(ERROR) << cur->str() << ": " << errmsg("sendto");
                cur->failures ++;
                if (cur->failures > max_failures) {
                    LOG(WARNING) << "remove " << cur->str() << " due to too many failures";
                    m_subscribers.erase(cur);
                }
            }
        }
    }

    void add_subscriber(const sockaddr_in& sa) {
        unique_lock<mutex> lock(m_lock);
        list<subscriber>::iterator it = find_if(m_subscribers.begin(), m_subscribers.end(), subscriber::if_same(sa));
        if (it != m_subscribers.end()) {
            it->failures = 0;
            return;
        }
        m_subscribers.push_back(subscriber(sa));
    }

    void del_subscriber(const sockaddr_in& sa) {
        unique_lock<mutex> lock(m_lock);
        m_subscribers.remove_if(subscriber::if_same(sa));
    }
};

class app {
public:
    app() {
        mqtt::initialize();

        http_server_t::options options(m_imghandler);
        options.reuse_address(true)
            .thread_pool(make_shared<boost::network::utils::thread_pool>())
            .port(sz2str(FLAGS_http_port));
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
            g->add_op("imagesrv", {"input", "id"}, {}, m_imghandler.op());
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
        thread streamer_thread([this] { run_streamer(); });
        thread caster_thread([this] { run_caster(); });
        udp.run(&m_dispatcher);
        httpsrv_thread.join();
        caster_thread.join();
        streamer_thread.join();
    }

private:
    unique_ptr<mqtt::client> m_mqtt_client;
    vector<unique_ptr<movidius::compute_stick>> m_ncs;
    vector<unique_ptr<movidius::compute_stick::graph>> m_models;
    vector<unique_ptr<graph>> m_graphs;
    graph_dispatcher m_dispatcher;
    image_handler m_imghandler;
    unique_ptr<http_server_t> m_httpsrv;

    void run_streamer() {
        if (FLAGS_stream_port == 0) {
            return;
        }
        streamer strm(FLAGS_stream_port);
        strm.run(&m_imghandler);
    }

    void run_caster() {
        if (FLAGS_stream_port == 0) {
            return;
        }
        udp_caster caster(FLAGS_stream_port);
        caster.run(&m_imghandler);
    }
};

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    app app;
    app.run();
    return 0;
}
