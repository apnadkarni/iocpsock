#include "iocpsockInt.h"
#include <af_irda.h>

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

static SocketInfo *	CreateIrdaSocket(Tcl_Interp *interp,
				CONST char *port, CONST char *host,
				int server, CONST char *myaddr,
				CONST char *myport, int async);


Tcl_Obj *
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
    int code, charSet, nameLen, size;
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

    sock = WSASocket(AF_IRDA, SOCK_STREAM, 0, NULL, 0,
	    WSA_FLAG_OVERLAPPED);

    if (sock == INVALID_SOCKET) {
	IocpWinConvertWSAError((DWORD) WSAGetLastError());
	Tcl_AppendResult(interp, "Cannot create IrDA socket: ",
		Tcl_PosixError(interp), NULL);
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
	IocpWinConvertWSAError((DWORD) WSAGetLastError());
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
    closesocket(sock);

    return TCL_OK;
}

int
Iocp_IrdaIasQuery (Tcl_Interp *interp, Tcl_Obj *deviceId,
	Tcl_Obj *serviceName, Tcl_Obj *attribName, Tcl_Obj **value)
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
	Tcl_AppendResult(interp, "Malformed IrDA DeviceID.  Must be in the form \"FF-FF-FF-FF.\"",
		NULL);
	return TCL_ERROR;
    }

    /*
     * First, make an IrDA socket.
     */

    sock = socket(AF_IRDA, SOCK_STREAM, 0);

    if (sock == INVALID_SOCKET) {
	IocpWinConvertWSAError((DWORD) WSAGetLastError());
	Tcl_AppendResult(interp, "Cannot create IrDA socket: ",
		Tcl_PosixError(interp), NULL);
	return TCL_ERROR;
    }

    if (attribName == NULL) {
	strcpy(iasQuery.irdaAttribName, "IrDA:IrLMP:LsapSel");
    } else {
	strncpy(iasQuery.irdaAttribName, Tcl_GetString(attribName), 256);
    }

    strncpy(iasQuery.irdaClassName, Tcl_GetString(serviceName), 64);

    code = getsockopt(sock, SOL_IRLMP, IRLMP_IAS_QUERY,
	    (char*) &iasQuery, &size);

    if (code == SOCKET_ERROR) {
	if (WSAGetLastError() != WSAECONNREFUSED) {
	    IocpWinConvertWSAError((DWORD) WSAGetLastError());
	    Tcl_AppendResult(interp, "getsockopt() failed: ",
		    Tcl_PosixError(interp), NULL);
	} else {
	    Tcl_AppendResult(interp, "No such service.", NULL);
	}
	closesocket(sock);
	return TCL_ERROR;
    }

    /*
     * Create the output Tcl_Obj, if none exists there.
     */

    if (*value == NULL) {
	*value = Tcl_NewObj();
    }

    closesocket(sock);

    switch (iasQuery.irdaAttribType) {
	case IAS_ATTRIB_INT:
	    Tcl_SetIntObj(*value, iasQuery.irdaAttribute.irdaAttribInt);
	    return TCL_OK;
	case IAS_ATTRIB_OCTETSEQ:
	    Tcl_SetByteArrayObj(*value, iasQuery.irdaAttribute.irdaAttribOctetSeq.OctetSeq,
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
		enc = Tcl_GetEncoding(interp, nameEnc);
		Tcl_ExternalToUtfDString(enc, iasQuery.irdaAttribute.irdaAttribUsrStr.UsrStr,
			iasQuery.irdaAttribute.irdaAttribUsrStr.Len, &deviceDString);
		Tcl_FreeEncoding(enc);
		Tcl_SetStringObj(*value, Tcl_DStringValue(&deviceDString),
			Tcl_DStringLength(&deviceDString));
		Tcl_DStringFree(&deviceDString);
	    }
	    return TCL_OK;
	case IAS_ATTRIB_NO_CLASS:
	    Tcl_SetStringObj(*value, "", -1);
	    return TCL_OK;
	case IAS_ATTRIB_NO_ATTRIB:
	    Tcl_SetStringObj(*value, "", -1);
	    return TCL_OK;
	default:
	    Tcl_Panic("No such arm.");
	    return TCL_ERROR;  /* makes compiler happy */
    }
}


int
Iocp_IrdaIasSet (Tcl_Interp *interp, Tcl_Obj *deviceId,
	Tcl_Obj *className, Tcl_Obj *attrName, Tcl_Obj *newValue)
{
    return TCL_ERROR;
}


int
Iocp_IrdaLazyDiscovery (Tcl_Interp *interp, Tcl_Obj *script)
{
    return TCL_ERROR;
}


Tcl_Channel
Iocp_OpenIrdaClient (
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    CONST char *ServiceName,	/* Service name on Device */
    CONST char *DeviceId,	/* Device on which to connect. */
    CONST char *myDeviceId,		/* Client-side address */
    CONST char *myServiceName,		/* Client-side port (number|service).*/
    int async)			/* If nonzero, should connect
				 * client socket asynchronously. */
{
    SocketInfo *infoPtr;
    char channelName[4 + TCL_INTEGER_SPACE];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateIrdaSocket(interp, ServiceName, DeviceId, 0, myDeviceId,
	    myServiceName, async);
    if (infoPtr == NULL) {
	return NULL;
    }
    snprintf(channelName, 4 + TCL_INTEGER_SPACE, "iocp%lu", infoPtr->socket);
    infoPtr->channel = Tcl_CreateChannel(&IocpChannelType, channelName,
	    (ClientData) infoPtr, (TCL_READABLE | TCL_WRITABLE));
    if (Tcl_SetChannelOption(interp, infoPtr->channel, "-translation",
	    "auto crlf") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
        return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption(NULL, infoPtr->channel, "-eofchar", "")
	    == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
        return (Tcl_Channel) NULL;
    }

    return infoPtr->channel;
}

Tcl_Channel
Iocp_OpenIrdaServer (
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    CONST char *serviceName,	/* Service name on Device */
    CONST char *DeviceId,	/* Device on which to open port. */
    CONST char *myDeviceId,		/* Client-side address */
    CONST char *myServiceName,		/* Client-side port (number|service).*/
    int async)			/* If nonzero, should connect
				 * client socket asynchronously. */
{
    SocketInfo *infoPtr;
    char channelName[4 + TCL_INTEGER_SPACE];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateIrdaSocket(interp, serviceName, DeviceId, 1, myDeviceId,
	    myServiceName, async);
    if (infoPtr == NULL) {
	return NULL;
    }
    snprintf(channelName, 4 + TCL_INTEGER_SPACE, "iocp%lu", infoPtr->socket);
    infoPtr->channel = Tcl_CreateChannel(&IocpChannelType, channelName,
	    (ClientData) infoPtr, (TCL_READABLE | TCL_WRITABLE));
    if (Tcl_SetChannelOption(interp, infoPtr->channel, "-translation",
	    "auto crlf") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
        return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption(NULL, infoPtr->channel, "-eofchar", "")
	    == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
        return (Tcl_Channel) NULL;
    }

    return infoPtr->channel;
}

static SocketInfo *
CreateIrdaSocket (
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    CONST char *serviceName,		/* Port to open. */
    CONST char *DeviceId,		/* Name of host on which to open port. */
    int server,			/* 1 if socket should be a server socket,
				 * else 0 for a client socket. */
    CONST char *myDeviceId,		/* Optional client-side address */
    CONST char *myServiceName,		/* Optional client-side port */
    int async)			/* If nonzero, connect client socket
				 * asynchronously. */
{
    return NULL;
}

#if 0
    u_long flag = 1;		/* Indicates nonblocking mode. */
    int asyncConnect = 0;	/* Will be 1 if async connect is
				 * in progress. */
    LPADDRINFO hostaddr;	/* Socket address */
    LPADDRINFO mysockaddr;	/* Socket address for client */
    SOCKET sock = INVALID_SOCKET;
    SocketInfo *infoPtr = NULL;	/* The returned value. */
    BufferInfo *bufPtr;		/* The returned value. */
    DWORD bytes;
    BOOL code;
    int i;
    WS2ProtocolData *pdata;
    ADDRINFO hints;
    LPADDRINFO addr;
    WSAPROTOCOL_INFO wpi;
    ThreadSpecificData *tsdPtr = InitSockets();

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    /* discover both/either ipv6 and ipv4. */
    if (host == NULL && !strcmp(port, "0")) {
	/* Win2K hack.  Ask for port 1, then set to 0 so getaddrinfo() doesn't bomb. */
	if (! CreateSocketAddress(host, "1", &hints, &hostaddr)) {
	    goto error1;
	}
	addr = hostaddr;
	while (addr) {
	    if (addr->ai_family == AF_INET) {
		((LPSOCKADDR_IN)addr->ai_addr)->sin_port = 0;
	    } else {
		IN6ADDR_SETANY((LPSOCKADDR_IN6) addr->ai_addr);
	    }
	    addr = addr->ai_next;
	}
    } else {
	if (! CreateSocketAddress(host, port, &hints, &hostaddr)) {
	    goto error1;
	}
    }
    addr = hostaddr;
    /* if we have more than one and being passive, choose ipv6. */
    if (addr->ai_next && host == NULL) {
	while (addr->ai_family != AF_INET6 && addr->ai_next) {
	    addr = addr->ai_next;
	}
    }

    if (myaddr != NULL || myport != NULL) {
	if (!CreateSocketAddress(myaddr, myport, addr, &mysockaddr)) {
	    goto error2;
	}
    } else if (!server) {
	/* Win2K hack.  Ask for port 1, then set to 0 so getaddrinfo() doesn't bomb. */
	if (!CreateSocketAddress(NULL, "1", addr, &mysockaddr)) {
	    goto error2;
	}
	if (mysockaddr->ai_family == AF_INET) {
	    ((LPSOCKADDR_IN)mysockaddr->ai_addr)->sin_port = 0;
	} else {
	    IN6ADDR_SETANY((LPSOCKADDR_IN6) mysockaddr->ai_addr);
	}
    }


    switch (addr->ai_family) {
    case AF_INET:
	pdata = &tcp4ProtoData; break;
    case AF_INET6:
	pdata = &tcp6ProtoData; break;
    default:
	Tcl_Panic("very bad protocol family returned from getaddrinfo()");
    }

    code = FindProtocolInfo(pdata->af, pdata->type, pdata->protocol,
	    0 /*XP1_QOS_SUPPORTED*/, &wpi);
    if (code == FALSE) {
	goto error2;
    }

    sock = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
	    FROM_PROTOCOL_INFO, &wpi, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET) {
	goto error2;
    }

    IocpInitProtocolData(sock, pdata);

    /*
     * Win-NT has a misfeature that sockets are inherited in child
     * processes by default.  Turn off the inherit bit.
     */

    SetHandleInformation((HANDLE)sock, HANDLE_FLAG_INHERIT, 0);

    /*
     * Turn off the internal send buffing.  We get more speed and are
     * more efficient by reducing memcpy calls as the stack will use
     * our overlapped buffers directly.
     */

    i = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
	    (const char *) &i, sizeof(int)) == SOCKET_ERROR) {
	goto error2;
    }

    infoPtr = NewSocketInfo(sock);
    infoPtr->proto = pdata;

    /* Info needed to get back to this thread. */
    infoPtr->tsdHome = tsdPtr;

    if (server) {

	/* Associate the socket and its SocketInfo struct to the completion
	 * port.  Implies an automatic set to non-blocking. */
	if (CreateIoCompletionPort((HANDLE)sock, IocpSubSystem.port,
		(ULONG_PTR)infoPtr, 0) == NULL) {
	    WSASetLastError(GetLastError());
	    goto error2;
	}

	/*
	 * Bind to the specified port.  Note that we must not call
	 * setsockopt with SO_REUSEADDR because Microsoft allows
	 * addresses to be reused even if they are still in use.
         *
         * Bind should not be affected by the socket having already been
         * set into nonblocking mode. If there is trouble, this is one
	 * place to look for bugs.
	 */
    
	if (bind(sock, addr->ai_addr,
		addr->ai_addrlen) == SOCKET_ERROR) {
            goto error2;
        }

	FreeSocketAddress(hostaddr);

        /*
         * Set the maximum number of pending connect requests to the
         * max value allowed on each platform (Win32 and Win32s may be
         * different, and there may be differences between TCP/IP stacks).
         */
        
	if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
	    goto error1;
	}

	/* create the queue for holding ready ones */
	infoPtr->readyAccepts = IocpLLCreate();

	/* post default IOCP_INITIAL_ACCEPT_COUNT accepts. */
        for(i=0; i < IOCP_ACCEPT_CAP ;i++) {
	    BufferInfo *bufPtr;
	    bufPtr = GetBufferObj(infoPtr, 0);
	    if (PostOverlappedAccept(infoPtr, bufPtr, 0) != NO_ERROR) {
		/* Oh no, the AcceptEx failed. */
		FreeBufferObj(bufPtr);
		goto error1;
	    }
        }

    } else {

        /*
         * bind to a local address.  ConnectEx needs this.
         */

	if (bind(sock, mysockaddr->ai_addr,
		mysockaddr->ai_addrlen) == SOCKET_ERROR) {
	    FreeSocketAddress(mysockaddr);
	    goto error2;
	}
	FreeSocketAddress(mysockaddr);

	/*
	 * Attempt to connect to the remote.
	 */

	if (async) {
	    bufPtr = GetBufferObj(infoPtr, 0);
	    bufPtr->operation = OP_CONNECT;

	    /* Associate the socket and its SocketInfo struct to the
	     * completion port.  Implies an automatic set to
	     * non-blocking. */
	    if (CreateIoCompletionPort((HANDLE)sock, IocpSubSystem.port,
		    (ULONG_PTR)infoPtr, 0) == NULL) {
		WSASetLastError(GetLastError());
		goto error2;
	    }

	    InterlockedIncrement(&infoPtr->outstandingOps);

	    code = pdata->ConnectEx(sock, addr->ai_addr,
		    addr->ai_addrlen, NULL, 0, &bytes, &bufPtr->ol);

	    FreeSocketAddress(hostaddr);

	    if (code == FALSE) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
		    InterlockedDecrement(&infoPtr->outstandingOps);
		    FreeBufferObj(bufPtr);
		    goto error1;
		}
	    }
	} else {
	    code = connect(sock, addr->ai_addr, addr->ai_addrlen);
	    FreeSocketAddress(hostaddr);
	    if (code == SOCKET_ERROR) {
		goto error1;
	    }

	    /* Associate the socket and its SocketInfo struct to the
	     * completion port.  Implies an automatic set to
	     * non-blocking. */
	    if (CreateIoCompletionPort((HANDLE)sock, IocpSubSystem.port,
		    (ULONG_PTR)infoPtr, 0) == NULL) {
		WSASetLastError(GetLastError());
		goto error1;
	    }

	    infoPtr->llPendingRecv = IocpLLCreate();

	    /* post IOCP_INITIAL_RECV_COUNT recvs. */
	    for(i=0; i < IOCP_INITIAL_RECV_COUNT ;i++) {
		bufPtr = GetBufferObj(infoPtr,
			(infoPtr->recvMode == IOCP_RECVMODE_ZERO_BYTE ? 0 : IOCP_RECV_BUFSIZE));
		PostOverlappedRecv(infoPtr, bufPtr, 0);
	    }
#if 0
	    {
		int ret;
		QOS clientQos;
		DWORD dwBytes;

		ZeroMemory(&clientQos, sizeof(QOS));
		clientQos.SendingFlowspec = flowspec_g711;
		clientQos.ReceivingFlowspec =  flowspec_notraffic;
		clientQos.ProviderSpecific.buf = NULL;
		clientQos.ProviderSpecific.len = 0;

		/*clientQos.SendingFlowspec.ServiceType |= 
			SERVICE_NO_QOS_SIGNALING;
		clientQos.ReceivingFlowspec.ServiceType |= 
			SERVICE_NO_QOS_SIGNALING;*/

		ret = WSAIoctl(sock, SIO_SET_QOS, &clientQos, 
		    sizeof(clientQos), NULL, 0, &dwBytes, NULL, NULL);
	    }
		bufPtr = GetBufferObj(infoPtr, QOS_BUFFER_SZ);
		PostOverlappedQOS(infoPtr, bufPtr);
#endif
	}
    }

    return infoPtr;

error2:
    FreeSocketAddress(hostaddr);
error1:
    SetLastError(WSAGetLastError());
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open socket: ",
		Tcl_Win32Error(interp), NULL);
    }
    FreeSocketInfo(infoPtr);
    return NULL;
}
#endif