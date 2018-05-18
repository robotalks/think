#include "mqtt/operators.h"

namespace mqtt::op {
    using namespace std;

    void pub::operator() (dp::graph::ctx ctx) {
        client->publish(topic, ctx.in(0)->as<string>());
    }
}
