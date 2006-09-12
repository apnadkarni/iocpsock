#include "iocpsockInt.h"
#include <wshisotp.h>

static FN_DECODEADDR DecodeIsoSockaddr;

static WS2ProtocolData isoProtoData = {
    AF_ISO,
    SOCK_STREAM,
    ISOPROTO_TP0,
    sizeof(SOCKADDR_TP),
    DecodeIsoSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static Tcl_Obj *
DecodeIsoSockaddr (SocketInfo *info, LPSOCKADDR addr)
{
    return Tcl_NewObj();
}

