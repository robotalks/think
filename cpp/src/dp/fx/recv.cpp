#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdexcept>
#include <glog/logging.h>

#include "dp/fx/recv.h"
#include "dp/util/error.h"

namespace dp::fx {
    using namespace std;

    sockaddr_in resolve_addr(const string& host, int port) {
        hostent* ent = gethostbyname(host.c_str());
        if (ent == nullptr || ent->h_addr_list[0] == nullptr) {
            throw runtime_error("unable to resolve " + host);
        }

        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = PF_INET;
        sa.sin_port = htons((u_short)port);
        sa.sin_addr = *(in_addr*)ent->h_addr_list[0];

        return sa;
    }

    socket_recv::socket_recv(int type)
    : m_socket(-1), m_buf(max_buf_size) {
        m_socket = socket(PF_INET, type, 0);
        if (m_socket < 0) {
            throw runtime_error(errmsg("socket"));
        }
    }

    socket_recv::~socket_recv() {
        if (m_socket >= 0) {
            close(m_socket);
        }
    }

    tcp_recv::tcp_recv(const string& host, int port)
    : socket_recv(SOCK_STREAM) {
        auto sa = resolve_addr(host, port);
        if (connect(socket_fd(), (sockaddr*)&sa, sizeof(sa)) < 0) {
            throw runtime_error(errmsg("connect"));
        }
    }

    void tcp_recv::run(recv_handler handler) {
        bool expect_data = false;
        size_t recv_size = 0, size = 0;
        while (true) {
            if (expect_data) {
                size_t left = size - recv_size;
                int r = recv(socket_fd(), (char*)buf_ptr()+recv_size, left, 0);
                if (r < 0) {
                    throw runtime_error(errmsg("recv"));
                }
                recv_size += (size_t)r;
                if (recv_size >= size) {
                    expect_data = false;
                    recv_size = 0;
                    handler(buf_ptr(), size);
                }
            } else {
                size_t left = 2 - recv_size;
                int r = recv(socket_fd(), (char*)buf_ptr()+recv_size, left, 0);
                if (r < 0) {
                    throw runtime_error(errmsg("recv"));
                }
                recv_size += (size_t)r;
                if (recv_size >= 2) {
                    size = *(uint16_t*)buf_ptr();
                    expect_data = true;
                    recv_size = 0;
                }
            }
        }
    }

    static constexpr __time_t recv_timeout = 5;

    static void heartbeat(int fd, const sockaddr_in& sa) {
        int r = sendto(fd, "+", 1, MSG_NOSIGNAL, (sockaddr*)&sa, sizeof(sa));
        if (r < 0) {
            LOG(ERROR) << errmsg("heartbeat");
        }
    }

    udp_recv::udp_recv(const string& host, int port)
    : socket_recv(SOCK_DGRAM) {
        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = PF_INET;
        sa.sin_addr.s_addr = INADDR_ANY;
        if (bind(socket_fd(), (sockaddr*)&sa, sizeof(sa)) < 0) {
            throw runtime_error(errmsg("bind"));
        }

        m_remote = resolve_addr(host, port);

        timeval rcvo;
        rcvo.tv_sec = recv_timeout;
        rcvo.tv_usec = 0;

        if (setsockopt(socket_fd(), SOL_SOCKET, SO_RCVTIMEO, &rcvo, sizeof(rcvo)) < 0) {
            throw runtime_error(errmsg("SO_RCVTIMEO"));
        }
    }

    void udp_recv::run(recv_handler handler) {
        heartbeat(socket_fd(), m_remote);
        while (true) {
            sockaddr_in sa;
            socklen_t salen = sizeof(sa);
            int r = recvfrom(socket_fd(), buf_ptr(), max_buf_size, 0, (sockaddr*)&sa, &salen);
            if (r < 0) {
                if (errno == EAGAIN) {
                    heartbeat(socket_fd(), m_remote);
                    continue;
                }
                throw runtime_error(errmsg("recvfrom"));
            }
            handler(buf_ptr(), (size_t)r);
        }
    }
}