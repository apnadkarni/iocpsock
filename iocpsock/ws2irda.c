#include "iocpsock.h"
#include <af_irda.h>

static FN_DECODEADDR DecodeIrdaSockaddr;

static WS2ProtocolData irdaProtoData = {
    AF_IRDA,
    SOCK_STREAM,
    IPPROTO_IP,
    sizeof(SOCKADDR_IRDA),
    DecodeIrdaSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static Tcl_Obj *
DecodeIrdaSockaddr (SocketInfo *info, LPSOCKADDR addr)
{
    return Tcl_NewObj();
}


#if 0
    SOCKADDR_IRDA     DestSockAddr = { AF_IRDA, 0, 0, 0, 0, "SampleIrDAService" };
 
    #define DEVICE_LIST_LEN    10

    unsigned char    DevListBuff[sizeof(DEVICELIST) -
	     sizeof(IRDA_DEVICE_INFO) +
	     (sizeof(IRDA_DEVICE_INFO) * DEVICE_LIST_LEN)];
    int        DevListLen    = sizeof(DevListBuff);
    PDEVICELIST    pDevList    = (PDEVICELIST) &DevListBuff;

    pDevList->numDevice = 0;

    // Sock is not in connected state
    if (getsockopt(Sock, SOL_IRLMP, IRLMP_ENUMDEVICES,
		   (char *) pDevList, &DevListLen) == SOCKET_ERROR)
    {
    // WSAGetLastError 
    }

    if (pDevList->numDevice == 0)
    {
	// no devices discovered or cached
	// not a bad idea to run a couple of times
    }
    else
    {
	// one per discovered device
    for (i = 0; i < (int) pDevList->numDevice; i++)
    {
    // typedef struct _IRDA_DEVICE_INFO
    // {
	    //    u_char    irdaDeviceID[4];
	    //    char    irdaDeviceName[22];
	    //    u_char    irdaDeviceHints1;
	    //     u_char    irdaDeviceHints2;
	    //    u_char    irdaCharSet;
    // } _IRDA_DEVICE_INFO;

	// pDevList->Device[i]. see _IRDA_DEVICE_INFO for fields
	// display the device names and let the user select one
    }
    }

    // assume the user selected the first device [0]
    memcpy(&DestSockAddr.irdaDeviceID[0], &pDevList->Device[0].irdaDeviceID[0], 4);

    if (connect(Sock, (const struct sockaddr *) &DestSockAddr,
		sizeof(SOCKADDR_IRDA)) == SOCKET_ERROR)
    {
	// WSAGetLastError
    }
#endif