#ifndef __MQTT_H
#define __MQTT_H

#include <string>
#include <future>
#include <mutex>
#include <list>
#include <unordered_map>
#include <functional>
#include <mosquitto.h>

namespace mqtt {
    void initialize();
    void cleanup();

    class client {
    public:
        using handler = ::std::function<void(
            const ::std::string& topic, const ::std::string& msg)>;

        static constexpr unsigned short default_port = 1883;

        client(const ::std::string& client_id = "", int keepalive = 60);
        virtual ~client();

        ::std::future<void> connect(const ::std::string& host, unsigned short port = default_port);
        ::std::future<void> disconnect();
        ::std::future<void> publish(const ::std::string& topic, const ::std::string& msg);
        ::std::future<void> subscribe(const ::std::string& topic, handler h);
        ::std::future<void> unsubscribe(const ::std::string& topic);

    private:
        ::std::string m_client_id;
        int m_keepalive;
        struct mosquitto *m_mosquitto;
        volatile ::std::promise<void>* m_conn_op;
        volatile ::std::promise<void>* m_disconn_op;
        volatile bool m_loop;
        ::std::mutex m_conn_lock;

        ::std::unordered_map<int, ::std::promise<void>*> m_pub_ops;
        ::std::mutex m_pub_ops_lock;

        ::std::unordered_map<::std::string, ::std::list<handler> > m_subs;
        ::std::mutex m_subs_lock;
        ::std::unordered_map<int, ::std::promise<void>*> m_sub_ops;
        ::std::mutex m_sub_ops_lock;

        void on_connect(int);
        void on_disconnect(int);
        void on_publish(int);
        void on_subscribe(int, int, const int*);
        void on_unsubscribe(int);
        void on_message(const struct mosquitto_message*);

        friend class callbacks;
    };
}

#endif
