#include "iocpsock.h"
#include <wsvns.h>

static WS2ProtocolData vinesProtoData = {
    AF_BAN,
    SOCK_STREAM,
    0,
    sizeof(SOCKADDR_VNS),
    NULL,
    NULL
};
