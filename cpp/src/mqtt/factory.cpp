#include <unordered_map>
#include <memory>
#include <string>

#include "dp/graph_def.h"
#include "mqtt/operators.h"

namespace mqtt::op {
    using namespace std;

    struct client_pool {
        unordered_map<string, unique_ptr<client>> clients;

        client* connect(const string& host, const string& client_id) {
            string key = host + "/" + client_id;
            auto it = clients.find(key);
            if (it != clients.end()) return it->second.get();
            string hostname = host;
            int port = 1883;
            auto pos = host.find_first_of(':');
            if (pos != string::npos) {
                hostname = host.substr(0, pos);
                port = atoi(host.substr(pos+1).c_str());
                if (pos <= 0 || port > 65535) {
                    throw invalid_argument("invalid port");
                }
            }
            unique_ptr<client> clt(new client(client_id));
            clt->connect(hostname, (unsigned short)port).wait();
            auto pr = clients.insert(make_pair(key, move(clt)));
            return pr.first->second.get();
        }
    };

    static client_pool _client_pool;

    template<typename T>
    struct topic_factory : dp::graph_def::op_factory {
        dp::graph::op_func create_op(
            const string& name, const string& type,
            const dp::graph_def::params& args) {
            auto host = args.at("host");
            if (host.empty()) throw invalid_argument("missing required parameter: host");
            auto client = _client_pool.connect(host, args.at("client_id"));
            auto topic = args.at("topic");
            return [client, topic] (dp::graph::ctx ctx) {
                T(client, topic)(ctx);
            };
        }
    };

    void register_factories() {
        static topic_factory<pub> pub_f;
        auto reg = dp::graph_def::op_registry::get();
        reg->add_factory("mqtt.pub", &pub_f);
    }
}
