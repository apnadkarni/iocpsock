#include "iocpsock.h"
#include <ws2atm.h>

static WS2ProtocolData atmProtoData = {
    AF_ATM,
    SOCK_STREAM,
    ATMPROTO_AAL5,
    sizeof(SOCKADDR_ATM),
    NULL,
    NULL
};
