#include "iocpsockInt.h"
#include <ws2dnet.h>

static FN_DECODEADDR DecodeDecnetSockaddr;

static WS2ProtocolData dnetProtoData = {
    AF_DECnet,
    SOCK_SEQPACKET,
    DNPROTO_NSP,
    sizeof(SOCKADDRDN),
    DecodeDecnetSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static Tcl_Obj *
DecodeDecnetSockaddr (SocketInfo *info, LPSOCKADDR addr)
{
    return Tcl_NewObj();
}