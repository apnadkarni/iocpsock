#include "iocpsockInt.h"
#include <wsvns.h>

static FN_DECODEADDR DecodeVinesSockaddr;

static WS2ProtocolData vinesProtoData = {
    AF_BAN,
    SOCK_STREAM,
    0,
    sizeof(SOCKADDR_VNS),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static Tcl_Obj *
DecodeVinesSockaddr (SocketInfo *info, LPSOCKADDR addr)
{
    return Tcl_NewObj();
}
