#include "iocpsock.h"
#include <ws2dnet.h>

static WS2ProtocolData dnetProtoData = {
    AF_DECnet,
    SOCK_SEQPACKET,
    DNPROTO_NSP,
    sizeof(SOCKADDRDN),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};
