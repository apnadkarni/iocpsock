#include "iocpsock.h"
#include <wsnetbs.h>

static WS2ProtocolData netbiosProtoData = {
    AF_NETBIOS,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_NB),
    NULL,
    NULL
};
