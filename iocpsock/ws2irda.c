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
    Tcl_Obj *result = Tcl_NewObj();
    return result;
}

int
Iocp_IrdaDiscovery (Tcl_Interp *interp, Tcl_Obj **deviceList, int limit)
{
    SOCKET sock;
    DEVICELIST *deviceListStruct;
    IRDA_DEVICE_INFO* thisDevice;
    int code, charSet, nameLen;
    unsigned i, bit;
    char isocharset[] = "iso-8859-?", *nameEnc;
    Tcl_Encoding enc;
    Tcl_Obj* entry[3];
    const char *hints1[] = {
	"PnP", "PDA", "Computer", "Printer", "Modem", "Fax", "LAN", NULL
    };
    const char *hints2[] = {
	"Telephony", "Server", "Comm", "Message", "HTTP", "OBEX", NULL
    };
    Tcl_DString deviceDString;

    /*
     * First make an IrDA socket.
     */

    sock = winSock.socket(AF_IRDA, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
	IocpWinConvertWSAError((DWORD) winSock.WSAGetLastError());
	Tcl_AppendResult(interp, "Cannot create IrDA socket: ",
		Tcl_PosixError(interp), NULL);
	return TCL_ERROR;
    }

    /*
     * Alloc the list we'll hand to getsockopt.
     */

    deviceListStruct = (DEVICELIST *) ckalloc(sizeof(DEVICELIST) - sizeof(IRDA_DEVICE_INFO)
	    + (sizeof(IRDA_DEVICE_INFO) * limit));

    code = winSock.getsockopt(sock, SOL_IRLMP, IRLMP_ENUMDEVICES,
	    (char*) deviceListStruct, &limit);

    if (code == SOCKET_ERROR) {
	IocpWinConvertWSAError((DWORD) winSock.WSAGetLastError());
	Tcl_AppendResult(interp, "getsockopt() failed: ",
		Tcl_PosixError(interp), NULL);
	ckfree((char *)deviceListStruct);
	return TCL_ERROR;
    }

    /*
     * Create the output Tcl_Obj, if none exists there.
     */
    if (*deviceList == NULL) {
	*deviceList = Tcl_NewObj();
    }

    for (i = 0; i < deviceListStruct->numDevice; i++) {
	thisDevice = deviceListStruct->Device+i;
	entry[0] = Tcl_NewLongObj(*((long*)thisDevice->irdaDeviceID));
	charSet = (thisDevice->irdaCharSet) & 0xff;
	switch (charSet) {
	    case 0xff:
		nameEnc = "unicode"; break;
	    case 0:
		nameEnc = "ascii"; break;
	    default:
		nameEnc = isocharset; 
		isocharset[9] = charSet+'0';
		break;
	}
	enc = Tcl_GetEncoding(interp, nameEnc);
	nameLen = (thisDevice->irdaDeviceName)[21] ? 22 :
		strlen(thisDevice->irdaDeviceName);
	Tcl_ExternalToUtfDString(enc, thisDevice->irdaDeviceName,
		nameLen, &deviceDString);
	Tcl_FreeEncoding(enc);
	entry[1] = Tcl_NewStringObj(Tcl_DStringValue(&deviceDString),
		Tcl_DStringLength(&deviceDString));
	Tcl_DStringFree(&deviceDString);
	entry[2] = Tcl_NewObj();
	for (bit=0; hints1[bit]; ++bit) 
	    if (thisDevice->irdaDeviceHints1 & (1<<bit))
		Tcl_ListObjAppendElement(interp,entry[2],
			Tcl_NewStringObj(hints1[bit],-1));
	for (bit=0; hints2[bit]; ++bit) 
	    if (thisDevice->irdaDeviceHints2 & (1<<bit))
		Tcl_ListObjAppendElement(interp,entry[2],
			Tcl_NewStringObj(hints2[bit],-1));
	Tcl_ListObjAppendElement(interp,*deviceList,
		Tcl_NewListObj(3,entry));
    }

    ckfree((char *)deviceListStruct);
    winSock.closesocket(sock);

    return TCL_OK;
}

Tcl_Channel
Iocp_OpenIrdaClient(
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    CONST char *DeviceId,	/* Device on which to open port. */
    CONST char *serviceName,	/* Service name */
    CONST char *myaddr,		/* Client-side address */
    CONST char *myport,		/* Client-side port (number|service).*/
    int async)			/* If nonzero, should connect
				 * client socket asynchronously. */
{
    return NULL;
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