#include "iocpsockInt.h"

static WS2ProtocolData udp4ProtoData = {
    AF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_IN),
    DecodeIpSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static WS2ProtocolData udp6ProtoData = {
    AF_INET6,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_IN6),
    DecodeIpSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static SocketInfo *	CreateUdpSocket(Tcl_Interp *interp,
				CONST char *port, CONST char *host,
				CONST char *myaddr, CONST char *myport);

/*
 *----------------------------------------------------------------------
 *
 * Iocp_OpenUdp4Client --
 *
 *	Opens a UDP socket and creates a channel around it.
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
Iocp_OpenUdpSocket (
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    CONST char *port,		/* Port (number|service) to open. */
    CONST char *host,		/* Host on which to open port. */
    CONST char *myaddr,		/* Client-side address */
    CONST char *myport)		/* Client-side port (number|service).*/
{
    SocketInfo *infoPtr;
    char channelName[4 + TCL_INTEGER_SPACE];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateUdpSocket(interp, port, host, myaddr, myport);
    if (infoPtr == NULL) {
	return NULL;
    }

    snprintf(channelName, 4 + TCL_INTEGER_SPACE, "iocp%d", infoPtr->socket);

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


#if 0
static SocketInfo *
CreateUdpSocket (
    Tcl_Interp *interp,
    CONST char *port,
    CONST char *host,
    CONST char *myaddr,
    CONST char *myport)
{
    LPADDRINFO hostaddr;	/* Socket address */
    LPADDRINFO mysockaddr;	/* Socket address for client */
    SOCKET sock = INVALID_SOCKET;
    SocketInfo *infoPtr;	/* The returned value. */
    BufferInfo *bufPtr;		/* The returned value. */
    DWORD bytes, WSAerr;
    BOOL code;
    int i;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);


    if (! CreateSocketAddress(host, port, &udp4ProtoData, &hostaddr)) {
	goto error;
    }
    if ((myaddr != NULL || myport != NULL) &&
	    ! CreateSocketAddress(myaddr, myport, &udp4ProtoData,
	    &mysockaddr)) {
	goto error;
    }

    sock = winSock.WSASocketA(udp4ProtoData.af, udp4ProtoData.type,
	    udp4ProtoData.protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET) {
	goto error;
    }

    /*
     * Win-NT has a misfeature that sockets are inherited in child
     * processes by default.  Turn off the inherit bit.
     */

    SetHandleInformation((HANDLE)sock, HANDLE_FLAG_INHERIT, 0);

    infoPtr = NewSocketInfo(sock);
    infoPtr->proto = &udp4ProtoData;

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
    
	if (winSock.bind(sock, hostaddr->ai_addr,
		hostaddr->ai_addrlen) == SOCKET_ERROR) {
            goto error;
        }

	FreeSocketAddress(hostaddr);

	/* create the queue for holding ready receives. */
	infoPtr->llPendingRecv = IocpLLCreate();

	bufPtr = GetBufferObj(infoPtr, 256);
	if (PostOverlappedRecv(infoPtr, bufPtr) != NO_ERROR) {
	    goto error;
        }

    } else {

        /*
         * Try to bind to a local port, if specified.
         */

	if (myaddr != NULL || myport != 0) { 
	    if (winSock.bind(sock, mysockaddr->ai_addr,
		    mysockaddr->ai_addrlen) == SOCKET_ERROR) {
		goto error;
	    }
	    FreeSocketAddress(mysockaddr);
	}            

	/*
	 * Attempt to connect to the remote.
	 */

	if (async) {
	    bufPtr = GetBufferObj(infoPtr, 0);
	    bufPtr->operation = OP_CONNECT;

	    code = tcp4ProtoData.ConnectEx(sock, hostaddr->ai_addr,
		    hostaddr->ai_addrlen, bufPtr->buf, bufPtr->buflen,
		    &bytes, &bufPtr->ol);

	    WSAerr = winSock.WSAGetLastError();
	    if (code == FALSE) {
		if (WSAerr != WSA_IO_PENDING) {
		    FreeBufferObj(bufPtr);
		    FreeSocketAddress(hostaddr);
		    goto error;
		}
	    }
	} else {
	    code = winSock.connect(sock, hostaddr->ai_addr, hostaddr->ai_addrlen);
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
#else
static SocketInfo *
CreateUdpSocket (
    Tcl_Interp *interp,
    CONST char *port,
    CONST char *host,
    CONST char *myaddr,
    CONST char *myport)
{
    return NULL;
}
#endif