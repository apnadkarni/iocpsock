#include "iocpsock.h"
#include <wsipx.h>
//#include <wsnwlink.h>	    NOT USED!

static WS2ProtocolData ipxProtoData = {
    AF_IPX,
    SOCK_DGRAM,
    NSPROTO_IPX,
    sizeof(SOCKADDR_IPX),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static WS2ProtocolData spxSequencedProtoData = {
    AF_IPX,
    SOCK_SEQPACKET,
    NSPROTO_SPX,
    sizeof(SOCKADDR_IPX),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static WS2ProtocolData spxStreamProtoData = {
    AF_IPX,
    SOCK_STREAM,
    NSPROTO_SPX,
    sizeof(SOCKADDR_IPX),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static WS2ProtocolData spx2SequencedProtoData = {
    AF_IPX,
    SOCK_SEQPACKET,
    NSPROTO_SPXII,
    sizeof(SOCKADDR_IPX),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static WS2ProtocolData spx2StreamProtoData = {
    AF_IPX,
    SOCK_STREAM,
    NSPROTO_SPXII,
    sizeof(SOCKADDR_IPX),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};
