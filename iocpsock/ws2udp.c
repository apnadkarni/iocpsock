#include "iocpsock.h"

static WS2ProtocolData udp4ProtoData = {
    AF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    NULL,
    NULL
};

static WS2ProtocolData udp6ProtoData = {
    AF_INET6,
    SOCK_DGRAM,
    IPPROTO_UDP,
    NULL,
    NULL
};
