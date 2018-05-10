#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "vp/operators.h"
#include "vp/ingress/udp.h"

namespace vp {
    using namespace std;

    static ::std::string errorMsg(const string& prefix) {
        char buf[32];
        sprintf(buf, ": %d", errno);
        return prefix + buf;
    }

    static constexpr size_t buf_size = 0x10000;

    UDPIngress::UDPIngress(uint16_t port, size_t buf_count)
    : m_pool(buf_count) {
        m_socket = socket(PF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0) {
            throw runtime_error(errorMsg("socket error"));
        }
        int en = 1;
        setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en));
        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = PF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = INADDR_ANY;
        if (bind(m_socket, (sockaddr*)&sa, sizeof(sa)) < 0) {
            throw runtime_error(errorMsg("bind error"));
        }

        m_bufs = new char[buf_size*buf_count];
    }

    UDPIngress::~UDPIngress() {
        if (m_socket >= 0) {
            close(m_socket);
        }
        delete (char*)m_bufs;
    }

    bool UDPIngress::prepareSession(Session& session, bool nowait) {
        size_t idx = m_pool.get(nowait);
        if (idx == IndexPool::none) return false;
        BufRef ref;
        ref.ptr = (char*)m_bufs + buf_size * idx;
        sockaddr_in sa;
        socklen_t salen = sizeof(sa);
        int r = recvfrom(m_socket, ref.ptr, buf_size, 0, (sockaddr*)&sa, &salen);
        if (r <= 0) return false;
        ref.len = (size_t)r;

        ImageId id;
        if (!ImageId::extract(ref, id)) {
            char str[64];
            sprintf(str, "%08x", sa.sin_addr.s_addr);
            id.src = str;
            ImageId::inject(ref, buf_size, id);
        }

        session.initializer = [this, ref] (Graph *g) {
            g->var(inputVar())->set<BufRef>(ref);
        };
        session.finalizer = [this, idx] (Graph *g) {
            m_pool.put(idx);
        };
        return true;
    }
}
