#include "iocpsock.h"

static GUID guidAcceptEx = WSAID_ACCEPTEX;
static GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

static WS2ProtocolData tcp4ProtoData = {
    AF_INET,
    SOCK_STREAM,
    IPPROTO_TCP,
    NULL,
    NULL
};

static WS2ProtocolData tcp6ProtoData = {
    AF_INET6,
    SOCK_STREAM,
    IPPROTO_TCP,
    NULL,
    NULL
};

static SocketInfo *	CreateTcp4Socket(Tcl_Interp *interp,
				    CONST char *port, CONST char *host,
				    int server, CONST char *myaddr,
				    CONST char *myport, int async);
static SocketInfo *	CreateTcp6Socket(Tcl_Interp *interp,
				    CONST char *port, CONST char *host,
				    int server, CONST char *myaddr,
				    int myport, int async);
//static int		  CreateSocketAddress(
//				    LPSOCKADDR_IN sockaddrPtr,
//				    CONST char *host, u_short port);
//static int		CreateSocketAddress(CONST char *addr,
//			    CONST char *port, WS2ProtocolData *pdata,
//			    ADDRINFO **result);


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
    return NULL;
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

    infoPtr = CreateTcp4Socket(interp, port, host, 1, NULL, 0, 0);
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

    return infoPtr->channel;
}

static SocketInfo *
CreateTcp4Socket(
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
    SocketInfo *infoPtr;	/* The returned value. */
    DWORD bytes;
    int i;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);


    if (! CreateSocketAddress(host, port, &tcp4ProtoData, &hostaddr)) {
	goto error;
    }
    if ((myaddr != NULL || myport != NULL) &&
	    ! CreateSocketAddress(myaddr, myport, &tcp4ProtoData,
	    &mysockaddr)) {
	goto error;
    }

    sock = winSock.WSASocketA(tcp4ProtoData.af, tcp4ProtoData.type,
	    tcp4ProtoData.protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET) {
	goto error;
    }

    /* is it cached? */
    if (tcp4ProtoData.AcceptEx == NULL) {
	/* Get the LSP specific functions. */
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&gAcceptExGuid, sizeof(GUID), &tcp4ProtoData.AcceptEx,
		sizeof(tcp4ProtoData.AcceptEx), &bytes, NULL, NULL);
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
               &gGetAcceptExSockaddrsGuid, sizeof(GUID),
               &tcp4ProtoData.GetAcceptExSockaddrs,
	       sizeof(tcp4ProtoData.GetAcceptExSockaddrs),
               &bytes, NULL, NULL);
    }

    /*
     * Win-NT has a misfeature that sockets are inherited in child
     * processes by default.  Turn off the inherit bit.
     */

    SetHandleInformation((HANDLE)sock, HANDLE_FLAG_INHERIT, 0);

    infoPtr = NewSocketInfo(sock);
    infoPtr->proto = &tcp4ProtoData;

    /* This calling thread is our target to get back to
     * when any accept/read/sends complete. */
    infoPtr->asyncToken = tsdPtr->asyncToken;
    infoPtr->threadId = tsdPtr->threadId;


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
    
	if (winSock.bind(sock, hostaddr->ai_addr,
		hostaddr->ai_addrlen) == SOCKET_ERROR) {
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

        // Keep track of the pending AcceptEx operations
        infoPtr->PendingAccepts = (BUFFER_OBJ **) HeapAlloc(
                IocpSubSystem.heap,
                HEAP_ZERO_MEMORY,
                (sizeof(BUFFER_OBJ *) * 20));

	/* pre-queue 20 accepts with the wsabufs only 128 bytes per. */
        for(i=0; i < 20 ;i++) {
	    BUFFER_OBJ *acceptobj;
            infoPtr->PendingAccepts[i] = acceptobj = GetBufferObj(infoPtr, 512);
            PostOverlappedAccept(infoPtr, acceptobj);
        }

    } else {

        /*
         * Try to bind to a local port, if specified.
         */
        
	if (myaddr != NULL || myport != 0) { 
	    if (winSock.bind(sock, (SOCKADDR *) &mysockaddr,
		    sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
		goto error;
	    }
	}            
    
	/*
	 * Set the socket into nonblocking mode if the connect should be
	 * done in the background.
	 */
    
//	if (async) {
	    if (winSock.ioctlsocket(sock, FIONBIO, &flag) == SOCKET_ERROR) {
		goto error;
	    }
//	}

	/*
	 * Attempt to connect to the remote socket.
	 */

	if (winSock.connect(sock, (SOCKADDR *) &hostaddr,
		sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
	    if (winSock.WSAGetLastError() != WSAEWOULDBLOCK) {
		goto error;
	    }

	    /*
	     * The connection is progressing in the background.
	     */

	    asyncConnect = 1;
        }

	/*
	 * Add this socket to the global list of sockets.
	 */

	infoPtr = NewSocketInfo(sock);

	/*
	 * Set up the select mask for read/write events.  If the connect
	 * attempt has not completed, include connect events.
	 */

//	infoPtr->selectEvents = FD_READ | FD_WRITE | FD_CLOSE;
	if (asyncConnect) {
//	    infoPtr->flags |= SOCKET_ASYNC_CONNECT;
//	    infoPtr->selectEvents |= FD_CONNECT;
	}
    }

    return infoPtr;

error:
    //TclWinConvertWSAError(winSock.WSAGetLastError());
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open socket: ", NULL);
	Tcl_AppendObjToObj(Tcl_GetObjResult(interp),
		GetSysMsgObj(winSock.WSAGetLastError()));
    }
    if (sock != INVALID_SOCKET) {
	winSock.closesocket(sock);
    }
    return NULL;
}


#if 0
static int
CreateSocketAddress(
    LPSOCKADDR_IN sockaddrPtr,	    /* Socket address */
    CONST char *host,		    /* Host.  NULL implies INADDR_ANY. */
    u_short port)		    /* Port number. */
{
    struct hostent *hostent;		/* Host database entry */
    struct in_addr addr;		/* For 64/32 bit madness */

    ZeroMemory(sockaddrPtr, sizeof(SOCKADDR_IN));
    sockaddrPtr->sin_family = AF_INET;
    sockaddrPtr->sin_port = winSock.htons(port);
    if (host == NULL) {
	addr.s_addr = INADDR_ANY;
    } else {
        addr.s_addr = winSock.inet_addr(host);
        if (addr.s_addr == INADDR_NONE) {
            hostent = winSock.gethostbyname(host); /* BUG: blocking!!! */
            if (hostent != NULL) {
                memcpy(&addr, hostent->h_addr, hostent->h_length);
            } else {
#ifdef	EHOSTUNREACH
                Tcl_SetErrno(EHOSTUNREACH);
#else
#ifdef ENXIO
                Tcl_SetErrno(ENXIO);
#endif
#endif
		return 0;	/* Error. */
	    }
	}
    }

    /*
     * NOTE: On 64 bit machines the assignment below is rumored to not
     * do the right thing. Please report errors related to this if you
     * observe incorrect behavior on 64 bit machines such as DEC Alphas.
     * Should we modify this code to do an explicit memcpy?
     */

    sockaddrPtr->sin_addr.s_addr = addr.s_addr;
    return 1;	/* Success. */
}
#endif