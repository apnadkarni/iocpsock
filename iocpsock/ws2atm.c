#include "iocpsock.h"
#include <ws2atm.h>

static FN_DECODEADDR DecodeAtmSockaddr;

static WS2ProtocolData atmProtoData = {
    AF_ATM,
    SOCK_STREAM,
    ATMPROTO_AAL5,
    sizeof(SOCKADDR_ATM),
    DecodeAtmSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static Tcl_Obj *
DecodeAtmSockaddr (SOCKET s, LPSOCKADDR addr)
{
    return NULL;
}