#include "iocpsock.h"
#include <atalkwsh.h>

static WS2ProtocolData apltalkProtoData = {
    AF_APPLETALK,
    SOCK_STREAM,
    ATPROTO_ATP,
    sizeof(SOCKADDR_AT),
    NULL,
    NULL,
    NULL
};
