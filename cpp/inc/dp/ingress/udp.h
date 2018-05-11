#ifndef __DP_INGRESS_UDP_H
#define __DP_INGRESS_UDP_H

#include "dp/ingress.h"
#include "dp/util/pool.h"

namespace dp::in {
    class udp : public ingress {
    public:
        static constexpr uint16_t default_port = 2052;

        udp(uint16_t port = default_port, size_t buf_count = 8);
        virtual ~udp();

    protected:
        virtual bool prepare_session(session&, bool nowait);

    private:
        int m_socket;
        void* m_bufs;
        index_pool m_pool;
    };
}

#endif
