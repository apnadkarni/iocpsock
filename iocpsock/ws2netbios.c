#include "iocpsockInt.h"
#include <wsnetbs.h>

static FN_DECODEADDR DecodeNBiosSockaddr;

static WS2ProtocolData netbiosProtoData = {
    AF_NETBIOS,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_NB),
    DecodeNBiosSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static Tcl_Obj *
DecodeNBiosSockaddr (SocketInfo *info, LPSOCKADDR addr)
{
    return Tcl_NewObj();
}
