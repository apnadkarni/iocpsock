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
    NULL,
    NULL,
    NULL
};

static SocketInfo *	CreateTcpSocket(Tcl_Interp *interp,
				CONST char *port, CONST char *host,
				int server, CONST char *myaddr,
				CONST char *myport, int async);

const FLOWSPEC flowspec_notraffic = {QOS_NOT_SPECIFIED,
                                     QOS_NOT_SPECIFIED,
                                     QOS_NOT_SPECIFIED,
                                     QOS_NOT_SPECIFIED,
                                     QOS_NOT_SPECIFIED,
                                     SERVICETYPE_NOTRAFFIC,
                                     QOS_NOT_SPECIFIED,
                                     QOS_NOT_SPECIFIED};

const FLOWSPEC flowspec_g711 = {8500,
                                680,
                                17000,
                                QOS_NOT_SPECIFIED,
                                QOS_NOT_SPECIFIED,
                                SERVICETYPE_CONTROLLEDLOAD,
                                340,
                                340};

const FLOWSPEC flowspec_guaranteed = {17000,
                                      1260,
                                      34000,
                                      QOS_NOT_SPECIFIED,
                                      QOS_NOT_SPECIFIED,
                                      SERVICETYPE_GUARANTEED,
                                      340,
                                      340};

/*
 *----------------------------------------------------------------------
 *
 * Iocp_OpenTcpClient --
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
    wsprintf(channelName, "iocp%lu", infoPtr->socket);
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
 * Iocp_OpenTcpServer --
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

    infoPtr = CreateTcpSocket(interp, port, host, 1 /*server*/, NULL, 0, 0);
    if (infoPtr == NULL) {
	return NULL;
    }

    infoPtr->acceptProc = acceptProc;
    infoPtr->acceptProcData = acceptProcData;
    wsprintf(channelName, "iocp%lu", infoPtr->socket);
    infoPtr->channel = Tcl_CreateChannel(&IocpChannelType, channelName,
	    (ClientData) infoPtr, 0);
    if (Tcl_SetChannelOption(interp, infoPtr->channel, "-eofchar", "")
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
    /* if we have more than one and being passive, choose ipv4. */
    if (addr->ai_next && host == NULL) {
	while (addr->ai_family != AF_INET && addr->ai_next) {
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
	    XP1_QOS_SUPPORTED, &wpi);

    sock = winSock.WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
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
    if (winSock.setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
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
	    winSock.WSASetLastError(GetLastError());
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
    
	if (winSock.bind(sock, addr->ai_addr,
		addr->ai_addrlen) == SOCKET_ERROR) {
            goto error2;
        }

	FreeSocketAddress(hostaddr);

        /*
         * Set the maximum number of pending connect requests to the
         * max value allowed on each platform (Win32 and Win32s may be
         * different, and there may be differences between TCP/IP stacks).
         */
        
	if (winSock.listen(sock, SOMAXCONN) == SOCKET_ERROR) {
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

	if (winSock.bind(sock, mysockaddr->ai_addr,
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
		winSock.WSASetLastError(GetLastError());
		goto error2;
	    }

	    InterlockedIncrement(&infoPtr->outstandingOps);

	    code = pdata->ConnectEx(sock, addr->ai_addr,
		    addr->ai_addrlen, NULL, 0, &bytes, &bufPtr->ol);

	    FreeSocketAddress(hostaddr);

	    if (code == FALSE) {
		if (winSock.WSAGetLastError() != WSA_IO_PENDING) {
		    InterlockedDecrement(&infoPtr->outstandingOps);
		    FreeBufferObj(bufPtr);
		    goto error1;
		}
	    }
	} else {
	    code = winSock.connect(sock, addr->ai_addr, addr->ai_addrlen);
	    FreeSocketAddress(hostaddr);
	    if (code == SOCKET_ERROR) {
		goto error1;
	    }

	    /* Associate the socket and its SocketInfo struct to the
	     * completion port.  Implies an automatic set to
	     * non-blocking. */
	    if (CreateIoCompletionPort((HANDLE)sock, IocpSubSystem.port,
		    (ULONG_PTR)infoPtr, 0) == NULL) {
		winSock.WSASetLastError(GetLastError());
		goto error1;
	    }

	    infoPtr->llPendingRecv = IocpLLCreate();

	    /* post IOCP_INITIAL_RECV_COUNT recvs. */
	    for(i=0; i < IOCP_INITIAL_RECV_COUNT ;i++) {
		bufPtr = GetBufferObj(infoPtr, IOCP_RECV_BUFSIZE);
		PostOverlappedRecv(infoPtr, bufPtr, 0);
	    }
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

		ret = winSock.WSAIoctl(sock, SIO_SET_QOS, &clientQos, 
		    sizeof(clientQos), NULL, 0, &dwBytes, NULL, NULL);
	    }
		bufPtr = GetBufferObj(infoPtr, QOS_BUFFER_SZ);
		PostOverlappedQOS(infoPtr, bufPtr);
	}
    }

    return infoPtr;

error2:
    FreeSocketAddress(hostaddr);
error1:
    IocpWinConvertWSAError(winSock.WSAGetLastError());
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open socket: ",
		Tcl_PosixError(interp), NULL);
    }
    FreeSocketInfo(infoPtr);
    return NULL;
}
