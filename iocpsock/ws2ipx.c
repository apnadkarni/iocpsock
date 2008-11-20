#include "iocpsockInt.h"
#include <wsipx.h>
//#include <wsnwlink.h>	    NOT USED!

static WS2ProtocolData ipxProtoData = {
    AF_IPX,
    SOCK_DGRAM,
    NSPROTO_IPX,
    sizeof(SOCKADDR_IPX),
    DecodeIpxSockaddr,
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
    DecodeIpxSockaddr,
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
    DecodeIpxSockaddr,
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
    DecodeIpxSockaddr,
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
    DecodeIpxSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

Tcl_Obj *
DecodeIpxSockaddr (SocketInfo *info, LPSOCKADDR addr)
{
    static char dest[128];
    LPSOCKADDR_IPX ipxadddr = (LPSOCKADDR_IPX) addr;
    snprintf(dest, 128, "%02X%02X%02X%02X.%02X%02X%02X%02X%02X%02X:%04X", 
        (unsigned char)ipxadddr->sa_netnum[0],
        (unsigned char)ipxadddr->sa_netnum[1],
        (unsigned char)ipxadddr->sa_netnum[2],
        (unsigned char)ipxadddr->sa_netnum[3],
        (unsigned char)ipxadddr->sa_nodenum[0],
        (unsigned char)ipxadddr->sa_nodenum[1],
        (unsigned char)ipxadddr->sa_nodenum[2],
        (unsigned char)ipxadddr->sa_nodenum[3],
        (unsigned char)ipxadddr->sa_nodenum[4],
        (unsigned char)ipxadddr->sa_nodenum[5],
        ntohs(ipxadddr->sa_socket));
    return Tcl_NewStringObj(dest, -1);
}
