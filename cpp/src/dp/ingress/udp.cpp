#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "dp/operators.h"
#include "dp/types.h"
#include "dp/ingress/udp.h"

namespace dp::in {
    using namespace std;

    static ::std::string error_msg(const string& prefix) {
        char buf[32];
        sprintf(buf, ": %d", errno);
        return prefix + buf;
    }

    static constexpr size_t buf_size = 0x10000;

    udp::udp(uint16_t port, size_t buf_count)
    : m_pool(buf_count) {
        m_socket = socket(PF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0) {
            throw runtime_error(error_msg("socket error"));
        }
        int en = 1;
        setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en));
        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = PF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = INADDR_ANY;
        if (bind(m_socket, (sockaddr*)&sa, sizeof(sa)) < 0) {
            throw runtime_error(error_msg("bind error"));
        }

        m_bufs = new char[buf_size*buf_count];
    }

    udp::~udp() {
        if (m_socket >= 0) {
            close(m_socket);
        }
        delete (char*)m_bufs;
    }

    bool udp::prepare_session(session& session, bool nowait) {
        size_t idx = m_pool.get(nowait);
        if (idx == index_pool::none) return false;
        buf_ref ref;
        ref.ptr = (char*)m_bufs + buf_size * idx;
        sockaddr_in sa;
        socklen_t salen = sizeof(sa);
        int r = recvfrom(m_socket, ref.ptr, buf_size, 0, (sockaddr*)&sa, &salen);
        if (r <= 0) return false;
        ref.len = (size_t)r;

        image_id id;
        if (!image_id::extract(ref, id)) {
            char str[64];
            sprintf(str, "%08x", sa.sin_addr.s_addr);
            id.src = str;
            image_id::inject(ref, buf_size, id);
        }

        session.initializer = [this, ref] (graph *g) {
            g->var(input_var())->set<buf_ref>(ref);
        };
        session.finalizer = [this, idx] (graph *g) {
            m_pool.put(idx);
        };
        return true;
    }
}
