#include "iocpsock.h"
#include <af_irda.h>

static WS2ProtocolData irdaProtoData = {
    AF_IRDA,
    SOCK_STREAM,
    IPPROTO_IP,
    NULL,
    NULL
};
