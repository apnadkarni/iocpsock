#include "iocpsock.h"

static WS2ProtocolData udp4ProtoData = {
    AF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_IN),
    NULL,
    NULL
};

static WS2ProtocolData udp6ProtoData = {
    AF_INET6,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_IN6),
    NULL,
    NULL
};
