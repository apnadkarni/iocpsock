#include "iocpsock.h"
#include <wshisotp.h>

static WS2ProtocolData isoProtoData = {
    AF_ISO,
    SOCK_STREAM,
    ISOPROTO_TP0,
    sizeof(SOCKADDR_TP),
    NULL,
    NULL,
    NULL,
    NULL
};
