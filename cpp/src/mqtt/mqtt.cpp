#include <cstring>
#include <stdexcept>
#include <glog/logging.h>

#include "mqtt/mqtt.h"

namespace mqtt {
    using namespace std;

    void initialize() {
        mosquitto_lib_init();
    }

    void cleanup() {
        mosquitto_lib_cleanup();
    }

    class callbacks {
    public:
        static void connect(struct mosquitto*, void* thiz, int rc) {
            reinterpret_cast<client*>(thiz)->on_connect(rc);
        }

        static void disconnect(struct mosquitto*, void* thiz, int rc) {
            reinterpret_cast<client*>(thiz)->on_disconnect(rc);
        }

        static void publish(struct mosquitto*, void* thiz, int mid) {
            reinterpret_cast<client*>(thiz)->on_publish(mid);
        }

        static void subscribe(struct mosquitto*, void* thiz, int mid, int qos, const int* granted_qos) {
            reinterpret_cast<client*>(thiz)->on_subscribe(mid, qos, granted_qos);
        }

        static void unsubscribe(struct mosquitto*, void* thiz, int mid) {
            reinterpret_cast<client*>(thiz)->on_unsubscribe(mid);
        }

        static void message(struct mosquitto*, void* thiz, const struct mosquitto_message *msg) {
            reinterpret_cast<client*>(thiz)->on_message(msg);
        }

        static void logging(struct mosquitto*, void* thiz, int level, const char* str) {
            VLOG(8) << "mqtt[" << level << "] " << str;
        }
    };

    static string make_error_msg(int rc, const string& errmsg) {
        char str[128];
        sprintf(str, "mqtt err %d: ", rc);
        return str + errmsg;
    }

    static future<void> make_promise(int rc, const string& errmsg, function<void(promise<void>*)> fn) {
        if (rc != MOSQ_ERR_SUCCESS) {
            promise<void> p;
            p.set_exception(make_exception_ptr(runtime_error(make_error_msg(rc, errmsg))));
            return p.get_future();
        }
        auto p = new promise<void>();
        fn(p);
        return p->get_future();
    }

    static void complete_promise(promise<void>* p, int rc, const string& errmsg) {
        if (p) {
            if (rc == MOSQ_ERR_SUCCESS) {
                p->set_value();
            } else {
                p->set_exception(make_exception_ptr(runtime_error(make_error_msg(rc, errmsg))));
            }
            delete p;
        }
    }

    static void complete_op(mutex& m, unordered_map<int, promise<void>*>& ops, int mid) {
        lock_guard<mutex> g(m);
        auto it = ops.find(mid);
        if (it != ops.end()) {
            it->second->set_value();
            ops.erase(it);
        }
    }

    client::client(const string& client_id, int keepalive)
    : m_client_id(client_id),
      m_keepalive(keepalive),
      m_conn_op(nullptr),
      m_disconn_op(nullptr),
      m_loop(false) {
        m_mosquitto = mosquitto_new(m_client_id.empty() ? nullptr : m_client_id.c_str(), true, this);
        mosquitto_connect_callback_set(m_mosquitto, callbacks::connect);
        mosquitto_disconnect_callback_set(m_mosquitto, callbacks::disconnect);
        mosquitto_publish_callback_set(m_mosquitto, callbacks::publish);
        mosquitto_subscribe_callback_set(m_mosquitto, callbacks::subscribe);
        mosquitto_unsubscribe_callback_set(m_mosquitto, callbacks::unsubscribe);
        mosquitto_message_callback_set(m_mosquitto, callbacks::message);
        mosquitto_log_callback_set(m_mosquitto, callbacks::logging);
    }

    client::~client() {
        mosquitto_destroy(m_mosquitto);
    }

    future<void> client::connect(const string& host, unsigned short port) {
        lock_guard<mutex> g(m_conn_lock);
        if (m_disconn_op) {
            throw logic_error("disconnect in-progress");
        }
        if (!m_conn_op) {
            return make_promise(
                mosquitto_connect_async(m_mosquitto, host.c_str(), port, m_keepalive),
                "connect error",
                [this] (promise<void>* p) {
                    m_conn_op = p;
                    if (!m_loop) {
                        mosquitto_loop_start(m_mosquitto);
                        m_loop = true;
                    }
                }
            );
        }
        return const_cast<promise<void>*>(m_conn_op)->get_future();
    }

    future<void> client::disconnect() {
        lock_guard<mutex> g(m_conn_lock);
        if (!m_disconn_op) {
            return make_promise(
                mosquitto_disconnect(m_mosquitto),
                "disconnect error",
                [this] (promise<void>* p) {
                    m_disconn_op = p;
                }
            );
        }
        return const_cast<promise<void>*>(m_disconn_op)->get_future();
    }

    future<void> client::publish(const string& topic, const string& data) {
        int mid = 0;
        return make_promise(
            mosquitto_publish(m_mosquitto, &mid, topic.c_str(),
                data.length(), data.c_str(),
                0, false),
            "publish error",
            [this, &mid] (promise<void>* p) {
                lock_guard<mutex> g(m_pub_ops_lock);
                m_pub_ops[mid] = p;
            }
        );
    }

    future<void> client::subscribe(const string& topic, handler callback) {
        {
            lock_guard<mutex> g(m_subs_lock);
            auto insertion = m_subs.insert(make_pair(topic, list<handler>()));
            insertion.first->second.push_back(callback);
            if (!insertion.second) {
                promise<void> p;
                p.set_value();
                return p.get_future();
            }
        }
        int mid = 0;
        return make_promise(
            mosquitto_subscribe(m_mosquitto, &mid, topic.c_str(), 0),
            "subscribe error",
            [this, &mid] (promise<void>* p) {
                lock_guard<mutex> g(m_sub_ops_lock);
                m_sub_ops[mid] = p;
            }
        );
    }

    future<void> client::unsubscribe(const string& topic) {
        {
            lock_guard<mutex> g(m_subs_lock);
            auto it = m_subs.find(topic);
            if (it == m_subs.end()) {
                promise<void> p;
                p.set_value();
                return p.get_future();
            }
            m_subs.erase(it);
        }
        int mid = 0;
        return make_promise(
            mosquitto_unsubscribe(m_mosquitto, &mid, topic.c_str()),
            "unsubscribe error",
            [this, &mid] (promise<void>* p) {
                lock_guard<mutex> g(m_sub_ops_lock);
                m_sub_ops[mid] = p;
            }
        );
    }

    void client::on_connect(int rc) {
        lock_guard<mutex> g(m_conn_lock);
        complete_promise((promise<void>*)m_conn_op, rc, "connect error");
        m_conn_op = nullptr;
    }

    void client::on_disconnect(int rc) {
        lock_guard<mutex> g(m_conn_lock);
        complete_promise((promise<void>*)m_disconn_op, rc, "disconnect error");
        m_disconn_op = nullptr;
        if (rc == 0) {
            mosquitto_loop_stop(m_mosquitto, false);
            m_loop = false;
        }
    }

    void client::on_publish(int mid) {
        complete_op(m_pub_ops_lock, m_pub_ops, mid);
    }

    void client::on_subscribe(int mid, int qos, const int* granted_qos) {
        complete_op(m_sub_ops_lock, m_sub_ops, mid);
    }

    void client::on_unsubscribe(int mid) {
        complete_op(m_sub_ops_lock, m_sub_ops, mid);
    }

    void client::on_message(const struct mosquitto_message* msg) {
        if (msg->topic == nullptr) {
            return;
        }
        list<handler> handlers;
        {
            lock_guard<mutex> g(m_subs_lock);
            for (auto v : m_subs) {
                bool result = false;
                int rc = mosquitto_topic_matches_sub(v.first.c_str(), msg->topic, &result);
                if (rc == MOSQ_ERR_SUCCESS && result) {
                    for (auto h : v.second) {
                        handlers.push_back(h);
                    }
                }
            }
        }
        if (!handlers.empty()) {
            string topic(msg->topic), message;
            message.assign(static_cast<char*>(msg->payload), msg->payloadlen);
            for (auto h : handlers) {
                h(topic, message);
            }
        }
    }
}
