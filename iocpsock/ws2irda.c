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
    char formatedId[12];
    Tcl_Obj *result = Tcl_NewObj();
    SOCKADDR_IRDA *irdaaddr = (SOCKADDR_IRDA *) addr;

    /* Device ID. */
    sprintf(formatedId, "%02x-%02x-%02x-%02x",
	    irdaaddr->irdaDeviceID[0], irdaaddr->irdaDeviceID[1],
	    irdaaddr->irdaDeviceID[2], irdaaddr->irdaDeviceID[3]);
    Tcl_ListObjAppendElement(NULL, result,
	    Tcl_NewStringObj(formatedId, 11));

    /* Service Name (probably not in UTF-8). */
    Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(
	    irdaaddr->irdaServiceName, -1));

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
    char formatedId[12];
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
	sprintf(formatedId, "%02x-%02x-%02x-%02x",
		thisDevice->irdaDeviceID[0], thisDevice->irdaDeviceID[1],
		thisDevice->irdaDeviceID[2], thisDevice->irdaDeviceID[3]);
	entry[0] = Tcl_NewStringObj(formatedId, 11);
	charSet = (thisDevice->irdaCharSet) & 0xff;
	switch (charSet) {
	    case 0xff:
		nameEnc = "unicode"; break;
	    case 0:
		nameEnc = "ascii"; break;
	    default:
		nameEnc = isocharset; 
		isocharset[9] = charSet + '0';
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
	for (bit=0; hints1[bit]; ++bit) {
	    if (thisDevice->irdaDeviceHints1 & (1<<bit))
		Tcl_ListObjAppendElement(interp, entry[2],
			Tcl_NewStringObj(hints1[bit],-1));
	}
	for (bit=0; hints2[bit]; ++bit) {
	    if (thisDevice->irdaDeviceHints2 & (1<<bit))
		Tcl_ListObjAppendElement(interp, entry[2],
			Tcl_NewStringObj(hints2[bit],-1));
	}
	Tcl_ListObjAppendElement(interp, *deviceList,
		Tcl_NewListObj(3, entry));
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
