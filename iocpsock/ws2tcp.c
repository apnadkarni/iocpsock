#include "iocpsock.h"

static WS2ProtocolData tcp4ProtoData = {
    AF_INET,
    SOCK_STREAM,
    IPPROTO_TCP,
    sizeof(SOCKADDR_IN),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static WS2ProtocolData tcp6ProtoData = {
    AF_INET6,
    SOCK_STREAM,
    IPPROTO_TCP,
    sizeof(SOCKADDR_IN6),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static SocketInfo *	CreateTcpSocket(Tcl_Interp *interp,
				CONST char *port, CONST char *host,
				int server, CONST char *myaddr,
				CONST char *myport, int async);

/*
 *----------------------------------------------------------------------
 *
 * Iocp_OpenTcp4Client --
 *
 *	Opens a TCP client socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed.  An error message is returned
 *	in the interpreter on failure.
 *
 * Side effects:
 *	Opens a client socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Iocp_OpenTcpClient(
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    CONST char *port,		/* Port (number|service) to open. */
    CONST char *host,		/* Host on which to open port. */
    CONST char *myaddr,		/* Client-side address */
    CONST char *myport,		/* Client-side port (number|service).*/
    int async)			/* If nonzero, should connect
				 * client socket asynchronously. */
{
    SocketInfo *infoPtr;
    char channelName[16 + TCL_INTEGER_SPACE];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateTcpSocket(interp, port, host, 0, myaddr, myport, async);
    if (infoPtr == NULL) {
	return NULL;
    }

    wsprintfA(channelName, "iocp%d", infoPtr->socket);

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
    // Had to add this!
    if (Tcl_SetChannelOption(NULL, infoPtr->channel, "-blocking", "0")
	    == TCL_ERROR) {
	Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
	return (Tcl_Channel) NULL;
    }
    return infoPtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Iocp_OpenTcp4Server --
 *
 *	Opens a TCP server socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed.  An error message is returned
 *	in the interpreter on failure.
 *
 * Side effects:
 *	Opens a server socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Iocp_OpenTcpServer(
    Tcl_Interp *interp,		/* For error reporting, may be NULL. */
    CONST char *port,		/* Port (number|service) to open. */
    CONST char *host,		/* Name of host for binding. */
    Tcl_TcpAcceptProc *acceptProc,
				/* Callback for accepting connections
				 * from new clients. */
    ClientData acceptProcData)	/* Data for the callback. */
{
    SocketInfo *infoPtr;
    char channelName[16 + TCL_INTEGER_SPACE];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateTcpSocket(interp, port, host, 1, NULL, 0, 0);
    if (infoPtr == NULL) {
	return NULL;
    }

    infoPtr->acceptProc = acceptProc;
    infoPtr->acceptProcData = acceptProcData;

    wsprintfA(channelName, "iocp%d", infoPtr->socket);

    infoPtr->channel = Tcl_CreateChannel(&IocpChannelType, channelName,
	    (ClientData) infoPtr, 0);
    if (Tcl_SetChannelOption(interp, infoPtr->channel, "-eofchar", "")
	    == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
        return (Tcl_Channel) NULL;
    }

    // Had to add this!
    if (Tcl_SetChannelOption(NULL, infoPtr->channel, "-blocking", "0")
	    == TCL_ERROR) {
	Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
	return (Tcl_Channel) NULL;
    }

    return infoPtr->channel;
}

static SocketInfo *
CreateTcpSocket(
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    CONST char *port,		/* Port to open. */
    CONST char *host,		/* Name of host on which to open port. */
    int server,			/* 1 if socket should be a server socket,
				 * else 0 for a client socket. */
    CONST char *myaddr,		/* Optional client-side address */
    CONST char *myport,		/* Optional client-side port */
    int async)			/* If nonzero, connect client socket
				 * asynchronously. */
{
    u_long flag = 1;		/* Indicates nonblocking mode. */
    int asyncConnect = 0;	/* Will be 1 if async connect is
				 * in progress. */
    LPADDRINFO hostaddr;	/* Socket address */
    LPADDRINFO mysockaddr;	/* Socket address for client */
    SOCKET sock = INVALID_SOCKET;
    SocketInfo *infoPtr = NULL;	/* The returned value. */
    BufferInfo *bufPtr;		/* The returned value. */
    DWORD bytes, WSAerr;
    BOOL code;
    int i;
    WS2ProtocolData *pdata;
    ADDRINFO hints;
    LPADDRINFO addr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    /* discover both/either ipv6 and ipv4. */
    if (! CreateSocketAddress(host, port, &hints, &hostaddr)) {
	goto error;
    }
    addr = hostaddr;
    /* if we have more than one and being passive, choose ipv4. */
    if (addr->ai_next && host == NULL) {
	while (addr->ai_family != AF_INET && addr->ai_next) {
	    addr = addr->ai_next;
	}
    }

    if ((myaddr != NULL || myport != NULL) &&
	    ! CreateSocketAddress(myaddr, myport, addr,
	    &mysockaddr)) {
	goto error;
    } else if (! CreateSocketAddress(NULL, "0", addr,
	    &mysockaddr)) {
	goto error;
    }


    switch (addr->ai_family) {
    case AF_INET:
	pdata = &tcp4ProtoData; break;
    case AF_INET6:
	pdata = &tcp6ProtoData; break;
    default:
	Tcl_Panic("very bad protocol family returned from getaddrinfo()");
    }

    sock = winSock.WSASocketA(pdata->af, pdata->type,
	    pdata->protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET) {
	goto error;
    }

    /* is it cached? */
    if (pdata->AcceptEx == NULL) {
	/* Get the LSP specific functions. */
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&AcceptExGuid, sizeof(GUID), 
		&pdata->AcceptEx,
		sizeof(pdata->AcceptEx),
		&bytes, NULL, NULL);
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
               &GetAcceptExSockaddrsGuid, sizeof(GUID),
               &pdata->GetAcceptExSockaddrs,
	       sizeof(pdata->GetAcceptExSockaddrs),
               &bytes, NULL, NULL);
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
               &ConnectExGuid, sizeof(GUID),
               &pdata->ConnectEx,
	       sizeof(pdata->ConnectEx),
               &bytes, NULL, NULL);
	if (pdata->ConnectEx == NULL) {
	    pdata->ConnectEx = OurConnectEx;
	}
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
               &DisconnectExGuid, sizeof(GUID),
               &pdata->DisconnectEx,
	       sizeof(pdata->DisconnectEx),
               &bytes, NULL, NULL);
	if (pdata->DisconnectEx == NULL) {
	    pdata->DisconnectEx = NULL;
	}
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
               &TransmitFileGuid, sizeof(GUID),
               &pdata->TransmitFile,
	       sizeof(pdata->TransmitFile),
               &bytes, NULL, NULL);
    }

    /*
     * Win-NT has a misfeature that sockets are inherited in child
     * processes by default.  Turn off the inherit bit.
     */

    SetHandleInformation((HANDLE)sock, HANDLE_FLAG_INHERIT, 0);

    infoPtr = NewSocketInfo(sock);
    infoPtr->proto = pdata;

    /* Info needed to get back to this thread. */
    infoPtr->tsdHome = tsdPtr;


    /* Associate the socket and its SocketInfo struct to the completion
     * port.  Implies an automatic set to non-blocking. */
    if (CreateIoCompletionPort((HANDLE)sock, IocpSubSystem.port,
	    (ULONG_PTR)infoPtr, 0) == NULL) {
	winSock.WSASetLastError(GetLastError());
	goto error;
    }

    if (server) {
	/*
	 * Bind to the specified port.  Note that we must not call
	 * setsockopt with SO_REUSEADDR because Microsoft allows
	 * addresses to be reused even if they are still in use.
         *
         * Bind should not be affected by the socket having already been
         * set into nonblocking mode. If there is trouble, this is one
	 * place to look for bugs.
	 */
    
	if (winSock.bind(sock, addr->ai_addr,
		addr->ai_addrlen) == SOCKET_ERROR) {
            goto error;
        }

	FreeSocketAddress(hostaddr);

        /*
         * Set the maximum number of pending connect requests to the
         * max value allowed on each platform (Win32 and Win32s may be
         * different, and there may be differences between TCP/IP stacks).
         */
        
	if (winSock.listen(sock, SOMAXCONN) == SOCKET_ERROR) {
	    goto error;
	}

        /* Keep track of the pending AcceptEx operations */
        infoPtr->llPendingAccepts = IocpLLCreate();

	/* create the queue for holding ready ones */
	infoPtr->readyAccepts = IocpLLCreate();

	IocpLLPushBack(IocpSubSystem.listeningSockets, infoPtr, &infoPtr->node);

	/* post IOCP_ACCEPT_COUNT accepts. */
        for(i=0; i < IOCP_ACCEPT_COUNT ;i++) {
	    BufferInfo *bufPtr;
	    bufPtr = GetBufferObj(infoPtr, IOCP_ACCEPT_BUFSIZE);
	    if (PostOverlappedAccept(infoPtr, bufPtr) != NO_ERROR) {
		/* Oh no, the AcceptEx failed. */
		FreeBufferObj(bufPtr);
		goto error;
	    }
        }

    } else {

        /*
         * bind to a local address.  ConnectEx needs this.
         */

	if (winSock.bind(sock, mysockaddr->ai_addr,
		mysockaddr->ai_addrlen) == SOCKET_ERROR) {
	    goto error;
	}
	FreeSocketAddress(mysockaddr);

	/*
	 * Attempt to connect to the remote.
	 */

	if (async) {
	    bufPtr = GetBufferObj(infoPtr, 0);
	    bufPtr->operation = OP_CONNECT;

	    code = pdata->ConnectEx(sock, addr->ai_addr,
		    addr->ai_addrlen, NULL, 0, &bytes, &bufPtr->ol);

	    WSAerr = winSock.WSAGetLastError();
	    if (code == FALSE) {
		if (WSAerr != WSA_IO_PENDING) {
		    FreeBufferObj(bufPtr);
		    FreeSocketAddress(hostaddr);
		    goto error;
		}
		InterlockedIncrement(&infoPtr->OutstandingOps);
	    }
	} else {
	    code = winSock.connect(sock, addr->ai_addr, addr->ai_addrlen);
	    if (code == SOCKET_ERROR) {
		FreeSocketAddress(hostaddr);
		goto error;
	    }
	}
	FreeSocketAddress(hostaddr);
    }

    return infoPtr;

error:
    IocpWinConvertWSAError(winSock.WSAGetLastError());
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open socket: ",
		Tcl_PosixError(interp), NULL);
    }
    FreeSocketInfo(infoPtr);
    return NULL;
}
