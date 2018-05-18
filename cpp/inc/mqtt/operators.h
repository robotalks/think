#ifndef __MQTT_OPERATORS_H
#define __MQTT_OPERATORS_H

#include "dp/graph.h"
#include "mqtt/mqtt.h"

namespace mqtt::op {
    struct pub {
        ::mqtt::client* client;
        ::std::string topic;
        pub(::mqtt::client* c, const ::std::string& t) : client(c), topic(t) {}
        void operator() (::dp::graph::ctx);
    };

    void register_factories();
}

#endif
