#include "iocpsock.h"
#include <atalkwsh.h>

static FN_DECODEADDR DecodeAplTlkSockaddr;

static WS2ProtocolData apltalkProtoData = {
    AF_APPLETALK,
    SOCK_STREAM,
    ATPROTO_ATP,
    sizeof(SOCKADDR_AT),
    DecodeAplTlkSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static Tcl_Obj *
DecodeAplTlkSockaddr (SOCKET s, LPSOCKADDR addr)
{
    return NULL;
}