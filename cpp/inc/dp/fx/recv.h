#ifndef __DP_FX_RECV_H
#define __DP_FX_RECV_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <functional>
#include <vector>
#include <string>
#include "dp/types.h"

namespace dp::fx {
    typedef ::std::function<void(void*, size_t)> recv_handler;

    struct receiver {
        virtual ~receiver() {}
        virtual void run(recv_handler) = 0;
    };

    constexpr int cast_port = 2053;

    sockaddr_in resolve_addr(const ::std::string& host, int port = 0);

    class socket_recv : public receiver {
    public:
        ~socket_recv();

        static constexpr size_t max_buf_size = 0x10000;

    protected:
        socket_recv(int type);

        int socket_fd() const { return m_socket; }
        void* buf_ptr() { return &m_buf[0]; }

    private:
        int m_socket;
        ::std::vector<uint8_t> m_buf;
    };

    class tcp_recv : public socket_recv {
    public:
        tcp_recv(const ::std::string& host, int port = cast_port);
        void run(recv_handler);
    };

    class udp_recv : public socket_recv {
    public:
        udp_recv(const ::std::string& host, int port = cast_port);
        void run(recv_handler);
    private:
        sockaddr_in m_remote;
    };
}

#endif
