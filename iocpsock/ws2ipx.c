#include "iocpsock.h"
#include <wsipx.h>
//#include <wsnwlink.h>	    NOT USED!

static WS2ProtocolData ipxProtoData = {
    AF_IPX,
    SOCK_DGRAM,
    NSPROTO_IPX,
    sizeof(SOCKADDR_IPX),
    NULL,
    NULL
};

