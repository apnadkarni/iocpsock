#include "tcl.h"
#include <errno.h>
#include <stdlib.h>

#ifdef __WIN32__
#   define WIN32_LEAN_AND_MEAN
    /* Enables NT5 special features. */
#   define _WIN32_WINNT 0x0501
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   include <wspiapi.h>
#   include "tclWinError.h"
#   pragma comment (lib, "ws2_32.lib")
#   ifdef _DEBUG
#	pragma comment (lib, "tcl86g.lib")
#   else
#	pragma comment (lib, "tcl86.lib")
#   endif
#endif


/* types */
enum Command {
    CMD_QUERY,	    /* resolve */
    CMD_REGISTER,   /* register a service */
    CMD_UNREGISTER, /* remove a service */
};
enum NameSpace {
    NS_INET,	/* IP unspecified, any (DNS) */
    NS_INET4,	/* IPv4 only (DNS) */
    NS_INET6,	/* IPv6 only (DNS) */
    NS_IRDA,	/* IrDA (IAS) */
    NS_BTH,	/* Bluetooth (SDP) */
    NS_IPX,	/* IPX/SPX (SAP) */
};

/* protos */
int Init (CONST char *appName);
void Finalize();
Tcl_FileProc StdinReadable;
void ParseNameProtocol (Tcl_Obj *line);
void SendStart(void);
void SendReady(void);
void SendProtocolError (int protocolCode, CONST char *msg);
void SendPosixErrorData (int protocolCode, CONST char *msg, int errorCode);
#ifdef __WIN32__
void SendWinErrorData (int protocolCode, CONST char *msg, DWORD errorCode);
#endif
void DoNameWork(enum Verb verb, enum NameSpace nameSpace, Tcl_Obj *arg1,
	       Tcl_Obj *arg2);
void Do_EnumerateNamespaces(void);


/* file scope globals */
int done = 0;
Tcl_Channel errChan, outChan, inChan;
Tcl_Obj *isIpRE_IPv4, *isIpRE_IPv6, *isIpRE_IPv6Comp, *isIpRE_4in6, *isIpRE_4in6Comp;


int
main (int argc, char *argv[])
{
    if (Init(argv[0]) == TCL_ERROR) {
         return EXIT_FAILURE;
    }
    while (!done) {
         Tcl_DoOneEvent(TCL_ALL_EVENTS);
    }
    Finalize();
    return EXIT_SUCCESS;
}

int
Init (CONST char *appName)
{
#ifdef __WIN32__
    WSADATA wsd;
    int err;
#endif

    Tcl_FindExecutable(appName);
    errChan = Tcl_GetStdChannel(TCL_STDERR);
    outChan = Tcl_GetStdChannel(TCL_STDOUT);
    inChan = Tcl_GetStdChannel(TCL_STDIN);

    Tcl_CreateChannelHandler(inChan, TCL_READABLE, StdinReadable, inChan);
    Tcl_SetChannelOption(NULL, inChan, "-buffering", "line");
    Tcl_SetChannelOption(NULL, inChan, "-blocking", "no");

    isIpRE_IPv4 = Tcl_NewStringObj("^((25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)(\\.(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)){3})$", -1);
    Tcl_IncrRefCount(isIpRE_IPv4);
    isIpRE_IPv6 = Tcl_NewStringObj("^((?:[[:xdigit:]]{1,4}:){7}[[:xdigit:]]{1,4})$", -1);
    Tcl_IncrRefCount(isIpRE_IPv6);
    isIpRE_IPv6Comp = Tcl_NewStringObj("^((?:[[:xdigit:]]{1,4}(?::[[:xdigit:]]{1,4})*)?)::((?:[[:xdigit:]]{1,4}(?::[[:xdigit:]]{1,4})*)?)$", -1);
    Tcl_IncrRefCount(isIpRE_IPv6Comp);
    isIpRE_4in6 = Tcl_NewStringObj("^(((?:[[:xdigit:]]{1,4}:){6,6})(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)(\\.(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)){3})$", -1);
    Tcl_IncrRefCount(isIpRE_4in6);
    isIpRE_4in6Comp = Tcl_NewStringObj("^(((?:[[:xdigit:]]{1,4}(?::[[:xdigit:]]{1,4})*)?)::((?:[[:xdigit:]]{1,4}:)*)(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)(\\.(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)){3})$", -1);
    Tcl_IncrRefCount(isIpRE_4in6Comp);

#ifdef __WIN32__
    if ((err = WSAStartup(MAKEWORD(2,2), &wsd)) != 0) {
	SendWinErrorData(500, "can't start winsock with WSAStartup()", err);
        return TCL_ERROR;
    }
#endif
    SendStart();
    SendReady();
    return TCL_OK;
}

void
Finalize()
{
    Tcl_DecrRefCount(isIpRE_IPv4);
    Tcl_DecrRefCount(isIpRE_IPv6);
    Tcl_DecrRefCount(isIpRE_IPv6Comp);
    Tcl_DecrRefCount(isIpRE_4in6);
    Tcl_DecrRefCount(isIpRE_4in6Comp);
    Tcl_Finalize();
#ifdef __WIN32__
    WSACleanup();
#endif
}

void
StdinReadable (ClientData clientData, int mask)
{
    Tcl_Channel inChan = (Tcl_Channel) clientData;
    Tcl_Obj *line = Tcl_NewObj();
    int count;

    count = Tcl_GetsObj(inChan, line);
    if (count > 0) {
	ParseNameProtocol(line);
    } else if (count == -1) {
	if (Tcl_Eof(inChan) == 0) {
	    /* not EOF, get the error code */
	    SendPosixErrorData(500, "Tcl_GetsObj() failed", Tcl_GetErrno());
	}
	/* cause the event loop to drop-out, thus exit */
	done = 1;
    }
    Tcl_DecrRefCount(line);
}

void
ParseNameProtocol (Tcl_Obj *line)
{
    int objc;
    Tcl_Obj **objv;
    char *cmdStrings[] = {
	"query", "register", "unregister", NULL
    };
    enum Command command;
    char *nsStrings[] = {
	"inet", "inet4", "inet6", "irda", "bth", "ipx", NULL
    };
    enum Namespace nameSpace;
    Tcl_Obj *arg1, *arg2 = NULL;

    /* form is "<command> <namespace> <arg1> [<arg2>]" using Tcl list rules. */
    if (Tcl_ListObjGetElements(NULL, line, &objc, &objv) == TCL_OK) {
	if (objc < 3 || objc > 5) {
	    SendProtocolError(600, "improper number of arguments");
	    return;
	}
	/* get command */
	if (Tcl_GetIndexFromObj(NULL, objv[0], cmdStrings, "", TCL_EXACT,
		(int *)&command) != TCL_OK) {
	    SendProtocolError(600, "no such command");
	    return;
	}
	/* get namespace */
	if (Tcl_GetIndexFromObj(NULL, objv[1], nsStrings, "", TCL_EXACT,
		(int *)&nameSpace) != TCL_OK) {
	    SendProtocolError(600, "no such namespace");
	    return;
	}
	/* get arg1 */
	arg1 = objv[2];
	/* get arg2, if any. */
	if (objc > 3) {
	    arg2 = objv[3];
	}
	DoNameWork(command, nameSpace, arg1, arg2);
	SendReady();
    }
}

void
SendStart(void)
{
    Tcl_Obj *output = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("Tcl name resolver version: 1.0", -1));
    Tcl_WriteObj(outChan, output);
    Tcl_DecrRefCount(output);
    Tcl_WriteChars(outChan, "\n", -1);
}

void
SendReady(void)
{
    Tcl_Obj *output = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(200));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("ready", -1));
    Tcl_WriteObj(outChan, output);
    Tcl_DecrRefCount(output);
    Tcl_WriteChars(outChan, "\n", -1);
}

void
SendAnswers(Tcl_Obj *question, Tcl_Obj *answers)
{
    Tcl_Obj *output = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(201));
    Tcl_ListObjAppendElement(NULL, output, question);
    Tcl_ListObjAppendElement(NULL, output, answers);
    Tcl_WriteObj(outChan, output);
    Tcl_WriteChars(outChan, "\n", -1);
    Tcl_DecrRefCount(output);
}

void
SendProtocolError (int protocolCode, CONST char *msg)
{
    Tcl_Obj *output = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(protocolCode));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(msg, -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("NAME", -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("", -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(0));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("", -1));
    Tcl_WriteObj(outChan, output);
    Tcl_DecrRefCount(output);
    Tcl_WriteChars(outChan, "\n", -1);
}

void
SendPosixErrorData (int protocolCode, CONST char *msg, int errorCode)
{
    Tcl_Obj *output = Tcl_NewObj();

    /* Assert this, should we be faking it for some purpose. */
    if (Tcl_GetErrno() != errorCode) {
	Tcl_SetErrno(errorCode);
    }
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(protocolCode));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(msg, -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("POSIX", -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(errorCode));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(Tcl_ErrnoId(), -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(Tcl_ErrnoMsg(errorCode), -1));
    Tcl_WriteObj(outChan, output);
    Tcl_DecrRefCount(output);
    Tcl_WriteChars(outChan, "\n", -1);
}

#ifdef __WIN32__
void
SendWinErrorData (int protocolCode, CONST char *msg, DWORD errorCode)
{
    Tcl_Obj *output = Tcl_NewObj();

    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(protocolCode));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(msg, -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("WINDOWS", -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(errorCode));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(Tcl_WinErrId(errorCode), -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewUnicodeObj(Tcl_WinErrMsg(errorCode, NULL), -1));
    Tcl_WriteObj(outChan, output);
    Tcl_DecrRefCount(output);
    Tcl_WriteChars(outChan, "\n", -1);
}
#endif

/***********************************************************************/

void Do_IP_Work (int addressFamily, Tcl_Obj *question);
int isIp (Tcl_Obj *name);
void Do_IrDA_Work (enum Command command, Tcl_Obj *question, Tcl_Obj *argument);
int Do_IrDA_Discovery (Tcl_Obj **answers);
int Do_IrDA_Query (Tcl_Obj *deviceId, Tcl_Obj *serviceName,
	Tcl_Obj *attribName, Tcl_Obj **answers);
void Do_Bth_Work (enum Command command, Tcl_Obj *question, Tcl_Obj *argument);
void Do_Bth_Query (Tcl_Obj *serviceName, Tcl_Obj **answers);
void Do_Ipx_Work (enum Command command, Tcl_Obj *question, Tcl_Obj *argument);

void
DoNameWork (enum Command command, enum NameSpace nameSpace,
    Tcl_Obj *arg1, Tcl_Obj *arg2)
{
    int aFamily = 0;

    switch (nameSpace) {
	case NS_INET:
	case NS_INET4:
	case NS_INET6:
	    switch (nameSpace) {
		case NS_INET:
		    aFamily = AF_UNSPEC; break;
		case NS_INET4:
		    aFamily = AF_INET; break;
		case NS_INET6:
		    aFamily = AF_INET6; break;
	    }
	    if (command != CMD_QUERY) {
		SendProtocolError(600, "The IP namespace only supports the query command");
		return;
	    }
	    Do_IP_Work(aFamily, arg1);
	    break;
	case NS_IRDA:
	    Do_IrDA_Work(command, arg1, arg2);
	    break;
	case NS_BTH:
	    /* BlueTooth */
	    Do_Bth_Work(command, arg1, arg2);
	    break;
	case NS_IPX:
	    /* BlueTooth */
	    Do_Ipx_Work(command, arg1, arg2);
	    break;
    }
}

void
Do_IP_Work (int addressFamily, Tcl_Obj *question)
{
    struct addrinfo hints;
    struct addrinfo *hostaddr, *addr;
    int result, type, len;
    CONST char *utf8Chars;
    Tcl_Obj *answers;
    Tcl_DString dnsTxt;
    Tcl_Encoding dnsEnc;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags  = 0;
    hints.ai_family = addressFamily;
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;

    /*
     * See:  ftp://ftp.rfc-editor.org/in-notes/rfc3490.txt
     */
    dnsEnc = Tcl_GetEncoding(NULL, "ascii");

    Tcl_DStringInit(&dnsTxt);
    utf8Chars = Tcl_GetStringFromObj(question, &len);
    Tcl_UtfToExternalDString(dnsEnc, utf8Chars, len, &dnsTxt);

    if ((result = getaddrinfo(Tcl_DStringValue(&dnsTxt), NULL, &hints,
	    &hostaddr)) != 0) {
#ifdef __WIN32__
	Tcl_DStringAppend(&dnsTxt, " failed to resolve.", -1);
	SendWinErrorData(405, Tcl_DStringValue(&dnsTxt), WSAGetLastError());
#else
	/* TODO */
#endif
	goto error1;
    }

    answers = Tcl_NewObj();

    if (isIp(question)) {
	/* question was a numeric IP, return a hostname. */
	type = NI_NAMEREQD;
    } else {
	/* question was a hostname, return a numeric IP. */
	type = NI_NUMERICHOST;
    }

    addr = hostaddr;
    while (addr != NULL) {
	char hostStr[NI_MAXHOST];
	int err;

	err = getnameinfo(addr->ai_addr, addr->ai_addrlen, hostStr,
		NI_MAXHOST, NULL, 0, type);

	if (err == 0) {
	    Tcl_ExternalToUtfDString(dnsEnc, hostStr, -1, &dnsTxt);
	    Tcl_ListObjAppendElement(NULL, answers,
		    Tcl_NewStringObj(Tcl_DStringValue(&dnsTxt),
		    Tcl_DStringLength(&dnsTxt)));
	} else {
#ifdef __WIN32__
	    SendWinErrorData(406, "lookup failed on getnameinfo()", WSAGetLastError());
#else
	    /* TODO */
#endif
	    goto error2;
	}
	addr = addr->ai_next;
    }

    /* reply with answers */
    SendAnswers(question, answers);

error2:
    freeaddrinfo(hostaddr);
error1:
    Tcl_DStringFree(&dnsTxt);
    return;
}

int
isIp (Tcl_Obj *name)
{
    int a, b, c, d, e;
    a = Tcl_RegExpMatchObj(NULL, name, isIpRE_IPv4);
    b = Tcl_RegExpMatchObj(NULL, name, isIpRE_IPv6);
    c = Tcl_RegExpMatchObj(NULL, name, isIpRE_IPv6Comp);
    d = Tcl_RegExpMatchObj(NULL, name, isIpRE_4in6);
    e = Tcl_RegExpMatchObj(NULL, name, isIpRE_4in6Comp);
    if (a || b || c || d || e) {
	return 1;
    }
    return 0;
}

void
Do_IrDA_Work (enum Command command, Tcl_Obj *question, Tcl_Obj *argument)
{
    int result, objc;
    Tcl_Obj **objv;
    Tcl_Obj *answers = NULL;

    switch (command) {
	case CMD_QUERY:
	    /* asterix means "get all", aka discovery.. */
	    if (strcmp(Tcl_GetString(question), "*") == 0) {
		if (Do_IrDA_Discovery(&answers) != TCL_OK) {
		    /* error msg already sent. */
		    return;
		}
	    } else {
		result = Tcl_ListObjGetElements(NULL, argument, &objc, &objv);
		if (result == TCL_OK && objc == 2) {
		    if (Do_IrDA_Query(question, objv[0], objv[1], &answers) != TCL_OK) {
			/* error msg already sent. */
			return;
		    }
		}
	    }
	    break;
	case CMD_REGISTER:
	case CMD_UNREGISTER:
	    /* TODO */
	    break;
    }
    /* reply with answers */
    SendAnswers(question, answers);
    return;
}

/* win specific */
#include <af_irda.h>

int
Do_IrDA_Discovery (Tcl_Obj **answers)
{
    SOCKET sock;
    DEVICELIST *deviceListStruct;
    IRDA_DEVICE_INFO* thisDevice;
    int code, nameLen, size, limit;
    unsigned int i, charSet, bit;
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

    /* dunno... */
    limit = 20;

    /*
     * First make an IrDA socket.
     */

    sock = WSASocket(AF_IRDA, SOCK_STREAM, 0, NULL, 0,
	    WSA_FLAG_OVERLAPPED);

    if (sock == INVALID_SOCKET) {
	SendWinErrorData(407, "Cannot create IrDA socket",
		WSAGetLastError());
	return TCL_ERROR;
    }

    /*
     * Alloc the list we'll hand to getsockopt.
     */

    size = sizeof(DEVICELIST) - sizeof(IRDA_DEVICE_INFO)
	    + (sizeof(IRDA_DEVICE_INFO) * limit);
    deviceListStruct = (DEVICELIST *) ckalloc(size);
    deviceListStruct->numDevice = 0;

    code = getsockopt(sock, SOL_IRLMP, IRLMP_ENUMDEVICES,
	    (char*) deviceListStruct, &size);

    if (code == SOCKET_ERROR) {
	SendWinErrorData(408, "getsockopt() failed", WSAGetLastError());
	ckfree((char *)deviceListStruct);
	return TCL_ERROR;
    }

    /*
     * Create the output Tcl_Obj, if none exists there.
     */

    if (*answers == NULL) {
	*answers = Tcl_NewObj();
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
	enc = Tcl_GetEncoding(NULL, nameEnc);
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
		Tcl_ListObjAppendElement(NULL, entry[2],
			Tcl_NewStringObj(hints1[bit],-1));
	}
	for (bit=0; hints2[bit]; ++bit) {
	    if (thisDevice->irdaDeviceHints2 & (1<<bit))
		Tcl_ListObjAppendElement(NULL, entry[2],
			Tcl_NewStringObj(hints2[bit],-1));
	}
	Tcl_ListObjAppendElement(NULL, *answers,
		Tcl_NewListObj(3, entry));
    }

    ckfree((char *)deviceListStruct);
    closesocket(sock);

    return TCL_OK;
}

int
Do_IrDA_Query (Tcl_Obj *deviceId, Tcl_Obj *serviceName,
	Tcl_Obj *attribName, Tcl_Obj **answers)
{
    SOCKET sock;
    int code, size = sizeof(IAS_QUERY);
    IAS_QUERY iasQuery;

    /*
     * Decode irdaDeviceId
     */
    code = sscanf(Tcl_GetString(deviceId), "%02x-%02x-%02x-%02x",
	&iasQuery.irdaDeviceID[0], &iasQuery.irdaDeviceID[1],
	&iasQuery.irdaDeviceID[2], &iasQuery.irdaDeviceID[3]);
    if (code != 4) {
	SendProtocolError(409, "Malformed IrDA DeviceID.  Must be in the form \"FF-FF-FF-FF.\"");
	return TCL_ERROR;
    }

    /*
     * First, make an IrDA socket.
     */

    sock = socket(AF_IRDA, SOCK_STREAM, 0);

    if (sock == INVALID_SOCKET) {
	SendWinErrorData(407, "Cannot create IrDA socket", WSAGetLastError());
	return TCL_ERROR;
    }

    strncpy(iasQuery.irdaAttribName, Tcl_GetString(attribName), 256);
    strncpy(iasQuery.irdaClassName, Tcl_GetString(serviceName), 64);

    code = getsockopt(sock, SOL_IRLMP, IRLMP_IAS_QUERY,
	    (char*) &iasQuery, &size);

    if (code == SOCKET_ERROR) {
	if (WSAGetLastError() != WSAECONNREFUSED) {
	    SendWinErrorData(408, "getsockopt() failed", WSAGetLastError());
	} else {
	    SendProtocolError(410, "No such service.");
	}
	closesocket(sock);
	return TCL_ERROR;
    }

    /*
     * Create the output Tcl_Obj, if none exists there.
     */

    if (*answers == NULL) {
	*answers = Tcl_NewObj();
    }

    closesocket(sock);

    switch (iasQuery.irdaAttribType) {
	case IAS_ATTRIB_INT:
	    Tcl_SetIntObj(*answers, iasQuery.irdaAttribute.irdaAttribInt);
	    return TCL_OK;
	case IAS_ATTRIB_OCTETSEQ:
	    Tcl_SetByteArrayObj(*answers, iasQuery.irdaAttribute.irdaAttribOctetSeq.OctetSeq,
		    iasQuery.irdaAttribute.irdaAttribOctetSeq.Len);
	    return TCL_OK;
	case IAS_ATTRIB_STR: {
		Tcl_Encoding enc;
		char isocharset[] = "iso-8859-?", *nameEnc;
		int charSet = iasQuery.irdaAttribute.irdaAttribUsrStr.CharSet  & 0xff;
		Tcl_DString deviceDString;
		switch (charSet) {
		    case 0xff:
			nameEnc = "unicode";
			break;
		    case 0:
			nameEnc = "ascii";
			break;
		    default:
			nameEnc = isocharset; 
			isocharset[9] = charSet + '0';
			break;
		}
		enc = Tcl_GetEncoding(NULL, nameEnc);
		Tcl_ExternalToUtfDString(enc, iasQuery.irdaAttribute.irdaAttribUsrStr.UsrStr,
			iasQuery.irdaAttribute.irdaAttribUsrStr.Len, &deviceDString);
		Tcl_FreeEncoding(enc);
		Tcl_SetStringObj(*answers, Tcl_DStringValue(&deviceDString),
			Tcl_DStringLength(&deviceDString));
		Tcl_DStringFree(&deviceDString);
	    }
	    return TCL_OK;
	case IAS_ATTRIB_NO_CLASS:
	    Tcl_SetStringObj(*answers, "no such class", -1);
	    return TCL_OK;
	case IAS_ATTRIB_NO_ATTRIB:
	    Tcl_SetStringObj(*answers, "no such attribute", -1);
	    return TCL_OK;
	default:
	    Tcl_Panic("No such arm.");
	    return TCL_ERROR;  /* makes compiler happy */
    }
}

#include <ws2bth.h>
//#include <BluetoothAPIs.h>
//#pragma comment ( lib, "Irprops.lib")

/* BlueTooth */
void
Do_Bth_Work (enum Command command, Tcl_Obj *question, Tcl_Obj *argument)
{
    int result, objc;
    Tcl_Obj **objv;
    Tcl_Obj *answers = NULL;

    switch (command) {
	case CMD_QUERY:
	    /* asterix means "get all", aka discovery.. */
	    if (strcmp(Tcl_GetString(question), "*") == 0) {
		if (Do_Bth_Discovery(&answers) != TCL_OK) {
		    /* error msg already sent. */
		    return;
		}
	    } else {
		result = Tcl_ListObjGetElements(NULL, argument, &objc, &objv);
		if (result == TCL_OK && objc == 2) {
		    if (Do_Bth_Query(question, objv[0], objv[1], &answers) != TCL_OK) {
			/* error msg already sent. */
			return;
		    }
		}
	    }
	    break;
	case CMD_REGISTER:
	case CMD_UNREGISTER:
	    /* TODO */
	    break;
    }
    /* reply with answers */
    SendAnswers(question, answers);
    return;
}

int
Do_Bth_Discovery (Tcl_Obj **answers)
{
}

//
// TODO: use inquiry timeout SDP_DEFAULT_INQUIRY_SECONDS
//

//
// NameToBthAddr converts a bluetooth device name to a bluetooth address, 
// if required by performing inquiry with remote name requests.
// This function demonstrates device inquiry, with optional LUP flags.
//
int
Do_Bth_Query(IN const char * pszRemoteName, OUT BTH_ADDR * pRemoteBtAddr)
{
    INT          iResult = 0, iRetryCount = 0;
    BOOL         bContinueLookup = FALSE, bRemoteDeviceFound = FALSE;
    ULONG        ulFlags = 0, ulPQSSize = sizeof(WSAQUERYSET);
    HANDLE       hLookup = 0;
    PWSAQUERYSET pWSAQuerySet = NULL;

    if ( ( NULL == pszRemoteName ) || ( NULL == pRemoteBtAddr ) )
    {
        goto CleanupAndExit;
    }

    if ( NULL == ( pWSAQuerySet = (PWSAQUERYSET) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ulPQSSize) ) )
    {
        printf("!ERROR! | Unable to allocate memory for WSAQUERYSET\n");
        goto CleanupAndExit;
    }

    //
    // Search for the device with the correct name
    //
    for (iRetryCount = 0; !bRemoteDeviceFound && (iRetryCount < CXN_MAX_INQUIRY_RETRY); iRetryCount++)
    {
        //
        // WSALookupService is used for both service search and device inquiry
        // LUP_CONTAINERS is the flag which signals that we're doing a device inquiry.  
        //    
        ulFlags = LUP_CONTAINERS;

        //
        // Friendly device name (if available) will be returned in lpszServiceInstanceName
        //
        ulFlags |= LUP_RETURN_NAME;

        //
        // BTH_ADDR will be returned in lpcsaBuffer member of WSAQUERYSET
        //
        ulFlags |= LUP_RETURN_ADDR;

        if ( 0 == iRetryCount )
        {
            printf("*INFO* | Inquiring device from cache...\n");
        }
        else
        {
            //
            // Flush the device cache for all inquiries, except for the first inquiry
            //
            // By setting LUP_FLUSHCACHE flag, we're asking the lookup service to do 
            // a fresh lookup instead of pulling the information from device cache.
            //
            ulFlags |= LUP_FLUSHCACHE;

            //
            // Pause for some time before all the inquiries after the first inquiry
            //
            //
            // BUGBUG why sleep. Try to get rid of this
            //
            // Remote Name requests will arrive after device inquiry has
            // completed.  Without a window to receive IN_RANGE notifications,
            // we don't have a direct mechanism to determine when remote
            // name requests have completed.
            //
            printf("*INFO* | Unable to find device.  Waiting for %d seconds before re-inquiry...\n", CXN_DELAY_NEXT_INQUIRY);
            Sleep(CXN_DELAY_NEXT_INQUIRY * 1000);

            printf("*INFO* | Inquiring device ...\n");
        }

        //
        // Start the lookup service
        //
        iResult = 0;
        hLookup = 0;
        bContinueLookup = FALSE;
        ZeroMemory(pWSAQuerySet, ulPQSSize);
        pWSAQuerySet->dwNameSpace = NS_BTH;
        pWSAQuerySet->dwSize = sizeof(WSAQUERYSET);
        iResult = WSALookupServiceBegin(pWSAQuerySet, ulFlags, &hLookup);

        if ( (NO_ERROR == iResult) && (NULL != hLookup) )
        {
            bContinueLookup = TRUE;
        }
        else if ( 0 < iRetryCount )
        {
            printf("=CRITICAL= | WSALookupServiceBegin() failed with error code %d, WSALastError = %d\n", iResult, WSAGetLastError());
            goto CleanupAndExit;
        }

        while ( bContinueLookup )
        {
            //
            // Get information about next bluetooth device
            // 
            // Note you may pass the same WSAQUERYSET from LookupBegin
            // as long as you don't need to modify any of the pointer
            // members of the structure, etc.
            //
            // ZeroMemory(pWSAQuerySet, ulPQSSize);
            // pWSAQuerySet->dwNameSpace = NS_BTH;
            // pWSAQuerySet->dwSize = sizeof(WSAQUERYSET);
            if ( NO_ERROR == WSALookupServiceNext(hLookup, ulFlags, &ulPQSSize, pWSAQuerySet) )
            {
                //
                // Since we're a non-unicode application, the remote
                // name in lpszServiceInstanceName will have been converted
                // from CP_UTF8 to CP_ACP, this may cause the name match
                // to fail unexpectedly.  If the app is to handle this,
                // the app needs to be unicode.
                //
                if ( ( pWSAQuerySet->lpszServiceInstanceName != NULL ) && ( 0 == _stricmp(pWSAQuerySet->lpszServiceInstanceName, pszRemoteName) ) )
                {
                    //
                    // Found a remote bluetooth device with matching name.
                    // Get the address of the device and exit the lookup.
                    //
                    CopyMemory(pRemoteBtAddr, 
                               &((PSOCKADDR_BTH) pWSAQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr)->btAddr, 
                               sizeof(*pRemoteBtAddr));
                    bRemoteDeviceFound = TRUE;
                    bContinueLookup = FALSE;
                }
            }
            else
            {
                if ( WSA_E_NO_MORE == ( iResult = WSAGetLastError() ) ) //No more data
                {
                    //
                    // No more devices found.  Exit the lookup.
                    //
                    bContinueLookup = FALSE;
                }
                else if ( WSAEFAULT == iResult )
                {
                    //
                    // The buffer for QUERYSET was insufficient.  
                    // In such case 3rd parameter "ulPQSSize" of function "WSALookupServiceNext()" receives 
                    // the required size.  So we can use this parameter to reallocate memory for QUERYSET.
                    //
                    HeapFree(GetProcessHeap(), 0, pWSAQuerySet);
                    pWSAQuerySet = NULL;
                    if ( NULL == ( pWSAQuerySet = (PWSAQUERYSET) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ulPQSSize) ) )
                    {
                        printf("!ERROR! | Unable to allocate memory for WSAQERYSET\n");
                        bContinueLookup = FALSE;
                    }
                }
                else
                {
                    printf("=CRITICAL= | WSALookupServiceNext() failed with error code %d\n", iResult);
                    bContinueLookup = FALSE;
                }
            }
        }

        //
        // End the lookup service
        //
        WSALookupServiceEnd(hLookup);
    }

CleanupAndExit:
    if ( NULL != pWSAQuerySet )
    {
        HeapFree(GetProcessHeap(), 0, pWSAQuerySet);
        pWSAQuerySet = NULL;
    }

    if ( bRemoteDeviceFound )
    {
        return(0);
    }
    else
    {
        return(1);
    }
}


#include <wsipx.h>

void
Do_Ipx_Work (enum Command command, Tcl_Obj *question, Tcl_Obj *argument)
{
}
