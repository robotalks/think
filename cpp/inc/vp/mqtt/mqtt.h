#ifndef __VP_MQTT_MQTT_H
#define __VP_MQTT_MQTT_H

#include <string>
#include <future>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <mosquitto.h>

namespace vp {
    struct MQTT {
        static void initialize();
        static void cleanup();
    };

    class MQTTClient {
    public:
        using Handler = ::std::function<void(
            const ::std::string& topic, const ::std::string& msg)>;

        static constexpr unsigned short PORT = 1883;

        MQTTClient(const ::std::string& clientId = "", int keepalive = 60);
        virtual ~MQTTClient();

        ::std::future<void> connect(const ::std::string& host, unsigned short port = PORT);
        ::std::future<void> disconnect();
        ::std::future<void> publish(const ::std::string& topic, const ::std::string& msg);
        ::std::future<void> subscribe(const ::std::string& topic, Handler handler);
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

        ::std::unordered_map<::std::string, ::std::list<Handler> > m_subs;
        ::std::mutex m_subs_lock;
        ::std::unordered_map<int, ::std::promise<void>*> m_sub_ops;
        ::std::mutex m_sub_ops_lock;

        void onConnect(int);
        void onDisconnect(int);
        void onPublish(int);
        void onSubscribe(int, int, const int*);
        void onUnsubscribe(int);
        void onMessage(const struct mosquitto_message*);

        friend class MQTTCallbacks;
    };
}

#endif
