#ifndef __VP_INGRESS_UDPRECV_H
#define __VP_INGRESS_UDPRECV_H

#include "vp/ingress.h"
#include "vp/util/pool.h"

namespace vp {
    class UDPIngress : public Ingress {
    public:
        static constexpr uint16_t PORT = 2052;

        UDPIngress(uint16_t port = PORT, size_t buf_count = 8);
        virtual ~UDPIngress();

    protected:
        virtual bool prepareSession(Session&, bool nowait);

    private:
        int m_socket;
        void* m_bufs;
        IndexPool m_pool;
    };
}

#endif
