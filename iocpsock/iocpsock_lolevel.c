// Think of this file as being win/tclWinSock.c

#include "iocpsock.h"
#include "iocpsock_util.h"
#include <crtdbg.h>

/*
 * The following declare the winsock loading error codes.
 */

static DWORD winsockLoadErr = 0;

#define TCL_WSE_NOTFOUND		1
#define TCL_WSE_CANTDOONEPOINTZERO	2
#define TCL_WSE_NOTALL11FUNCS		3
#define TCL_WSE_NOTALL2XFUNCS		4
#define TCL_WSE_CANTSTARTHANDLERTHREAD	5


/* some globals defined. */
WinsockProcs winSock;
int initialized = 0;
CompletionPortInfo IocpSubSystem;
GUID gAcceptExGuid = WSAID_ACCEPTEX;
GUID gGetAcceptExSockaddrsGuid = WSAID_GETACCEPTEXSOCKADDRS;

/* file scope globals. */
static DWORD numCPUs;


/* local prototypes */
static void			InitSockets();
static DWORD			InitializeIocpSubSystem();
static Tcl_ExitProc		IocpExitHandler;
static Tcl_ExitProc		IocpThreadExitHandler;
static Tcl_AsyncProc		IocpAsyncCallback;
static Tcl_EventProc		IocpEventCallback;

static Tcl_DriverCloseProc	IocpCloseProc;
static Tcl_DriverInputProc	IocpInputProc;
static Tcl_DriverOutputProc	IocpOutputProc;
static Tcl_DriverSetOptionProc	IocpSetOptionProc;
static Tcl_DriverGetOptionProc	IocpGetOptionProc;
static Tcl_DriverWatchProc	IocpWatchProc;
static Tcl_DriverGetHandleProc	IocpGetHandleProc;
static Tcl_DriverBlockModeProc	IocpBlockProc;

static void		    FreeBufferObj(BUFFER_OBJ *obj);
static void		    FreeSocketInfo(SocketInfo *si);
static DWORD		    PostOverlappedRecv(SocketInfo *si,
				    BUFFER_OBJ *recvobj);
static DWORD		    PostOverlappedSend(SocketInfo *si,
				    BUFFER_OBJ *sendobj);
static void		    InsertPendingSend(SocketInfo *si,
				    BUFFER_OBJ *send);
static int		    DoSends(SocketInfo *si);
static void		    HandleIo(SocketInfo *si, BUFFER_OBJ *buf,
				    HANDLE CompPort,
				    DWORD BytesTransfered, DWORD error,
				    DWORD flags);
static DWORD WINAPI	    CompletionThread(LPVOID lpParam);


/*
 * This structure describes the channel type structure for TCP socket
 * based I/O using the native overlapped interface along with completion
 * ports for maximum efficiency so that most operation are done entirely
 * in kernel-mode.
 */

Tcl_ChannelType IocpChannelType = {
    "tcp_iocp",		    /* Type name. */
    TCL_CHANNEL_VERSION_2,  /* v2 channel */
    IocpCloseProc,	    /* Close proc. */
    IocpInputProc,	    /* Input proc. */
    IocpOutputProc,	    /* Output proc. */
    NULL,		    /* Seek proc. */
    IocpSetOptionProc,	    /* Set option proc. */
    IocpGetOptionProc,	    /* Get option proc. */
    IocpWatchProc,	    /* Set up notifier to watch this channel. */
    IocpGetHandleProc,	    /* Get an OS handle from channel. */
    NULL,		    /* close2proc. */
    IocpBlockProc,	    /* Set socket into (non-)blocking mode. */
    NULL,		    /* flush proc. */
    NULL,		    /* handler proc. */
};


typedef struct SocketEvent {
    Tcl_Event header;		/* Information that is standard for
				 * all events. */
    SocketInfo *infoPtr;
} SocketEvent;


static void
InitSockets()
{
    WSADATA wsaData;
    OSVERSIONINFO os;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (!initialized) {
	initialized = 1;

	ZeroMemory(&winSock, sizeof(WinsockProcs));

	/*
	 * Try ws2_32.dll first, if available.
	 */

	if (winSock.hModule = LoadLibraryA("ws2_32.dll"));
	else if (winSock.hModule = LoadLibraryA("wsock32.dll"));
	else if (winSock.hModule = LoadLibraryA("winsock.dll"));
	else {
	    winsockLoadErr = TCL_WSE_NOTFOUND;
	    return;
	}

	/*
	 * Initialize the 1.1 half of the function table.
	 */

	winSock.accept = (LPFN_ACCEPT)
		GetProcAddress(winSock.hModule, "accept");
	winSock.bind = (LPFN_BIND)
		GetProcAddress(winSock.hModule, "bind");
	winSock.closesocket = (LPFN_CLOSESOCKET)
		GetProcAddress(winSock.hModule, "closesocket");
	winSock.connect = (LPFN_CONNECT)
		GetProcAddress(winSock.hModule, "connect");
	winSock.gethostbyaddr = (LPFN_GETHOSTBYADDR)
		GetProcAddress(winSock.hModule, "gethostbyaddr");
	winSock.gethostbyname = (LPFN_GETHOSTBYNAME)
		GetProcAddress(winSock.hModule, "gethostbyname");
	winSock.gethostname = (LPFN_GETHOSTNAME)
		GetProcAddress(winSock.hModule, "gethostname");
	winSock.getpeername = (LPFN_GETPEERNAME)
		GetProcAddress(winSock.hModule, "getpeername");
	winSock.getservbyname = (LPFN_GETSERVBYNAME)
		GetProcAddress(winSock.hModule, "getservbyname");
	winSock.getservbyport = (LPFN_GETSERVBYPORT)
		GetProcAddress(winSock.hModule, "getservbyport");
	winSock.getsockname = (LPFN_GETSOCKNAME)
		GetProcAddress(winSock.hModule, "getsockname");
	winSock.getsockopt = (LPFN_GETSOCKOPT)
		GetProcAddress(winSock.hModule, "getsockopt");
	winSock.htonl = (LPFN_HTONL)
		GetProcAddress(winSock.hModule, "htonl");
	winSock.htons = (LPFN_HTONS)
		GetProcAddress(winSock.hModule, "htons");
	winSock.inet_addr = (LPFN_INET_ADDR)
		GetProcAddress(winSock.hModule, "inet_addr");
	winSock.inet_ntoa = (LPFN_INET_NTOA)
		GetProcAddress(winSock.hModule, "inet_ntoa");
	winSock.ioctlsocket = (LPFN_IOCTLSOCKET)
		GetProcAddress(winSock.hModule, "ioctlsocket");
	winSock.listen = (LPFN_LISTEN)
		GetProcAddress(winSock.hModule, "listen");
	winSock.ntohs = (LPFN_NTOHS)
		GetProcAddress(winSock.hModule, "ntohs");
	winSock.recv = (LPFN_RECV)
		GetProcAddress(winSock.hModule, "recv");
	winSock.recvfrom = (LPFN_RECVFROM)
		GetProcAddress(winSock.hModule, "recvfrom");
	winSock.select = (LPFN_SELECT)
		GetProcAddress(winSock.hModule, "select");
	winSock.send = (LPFN_SEND)
		GetProcAddress(winSock.hModule, "send");
	winSock.sendto = (LPFN_SENDTO)
		GetProcAddress(winSock.hModule, "sendto");
	winSock.setsockopt = (LPFN_SETSOCKOPT)
		GetProcAddress(winSock.hModule, "setsockopt");
	winSock.shutdown = (LPFN_SHUTDOWN)
		GetProcAddress(winSock.hModule, "shutdown");
	winSock.socket = (LPFN_SOCKET)
		GetProcAddress(winSock.hModule, "socket");
	winSock.WSAAsyncSelect = (LPFN_WSAASYNCSELECT)
		GetProcAddress(winSock.hModule, "WSAAsyncSelect");
	winSock.WSACleanup = (LPFN_WSACLEANUP)
		GetProcAddress(winSock.hModule, "WSACleanup");
	winSock.WSAGetLastError = (LPFN_WSAGETLASTERROR)
		GetProcAddress(winSock.hModule, "WSAGetLastError");
	winSock.WSASetLastError = (LPFN_WSASETLASTERROR)
		GetProcAddress(winSock.hModule, "WSASetLastError");
	winSock.WSAStartup = (LPFN_WSASTARTUP)
		GetProcAddress(winSock.hModule, "WSAStartup");
    
	/*
	 * Now check that all fields are properly initialized. If not,
	 * return zero to indicate that we failed to initialize
	 * properly.
	 */
    
	if ((winSock.accept == NULL) ||
		(winSock.bind == NULL) ||
		(winSock.closesocket == NULL) ||
		(winSock.connect == NULL) ||
		(winSock.gethostbyname == NULL) ||
		(winSock.gethostbyaddr == NULL) ||
		(winSock.gethostname == NULL) ||
		(winSock.getpeername == NULL) ||
		(winSock.getservbyname == NULL) ||
		(winSock.getservbyport == NULL) ||
		(winSock.getsockname == NULL) ||
		(winSock.getsockopt == NULL) ||
		(winSock.htonl == NULL) ||
		(winSock.htons == NULL) ||
		(winSock.inet_addr == NULL) ||
		(winSock.inet_ntoa == NULL) ||
		(winSock.ioctlsocket == NULL) ||
		(winSock.listen == NULL) ||
		(winSock.ntohs == NULL) ||
		(winSock.recv == NULL) ||
		(winSock.recvfrom == NULL) ||
		(winSock.select == NULL) ||
		(winSock.send == NULL) ||
		(winSock.sendto == NULL) ||
		(winSock.setsockopt == NULL) ||
		(winSock.shutdown == NULL) ||
		(winSock.socket == NULL) ||
		(winSock.WSAAsyncSelect == NULL) ||
		(winSock.WSACleanup == NULL) ||
		(winSock.WSAGetLastError == NULL) ||
		(winSock.WSASetLastError == NULL) ||
		(winSock.WSAStartup == NULL)) {
	    winsockLoadErr = TCL_WSE_NOTALL11FUNCS;
	    goto unloadLibrary;
	}
	
	/*
	 * Initialize the winsock library and check the interface
	 * version number.  We ask for the 2.2 interface, but
	 * don't accept less than 1.1.
	 */

#define WSA_VER_MIN_MAJOR   1
#define WSA_VER_MIN_MINOR   1
#define WSA_VERSION_REQUESTED    MAKEWORD(2,2)

	if ((winsockLoadErr = winSock.WSAStartup(WSA_VERSION_REQUESTED,
		&wsaData)) != 0) {
	    goto unloadLibrary;
	}

	winSock.wVersionLoaded = wsaData.wVersion;

	/*
	 * Note the byte positions are swapped for the comparison, so
	 * that 0x0002 (2.0, MAKEWORD(2,0)) doesn't look less than 0x0101
	 * (1.1).  We want the comparison to be 0x0200 < 0x0101.
	 */
	if (MAKEWORD(HIBYTE(wsaData.wVersion), LOBYTE(wsaData.wVersion))
		< MAKEWORD(WSA_VER_MIN_MINOR, WSA_VER_MIN_MAJOR)) {
	    winsockLoadErr = TCL_WSE_CANTDOONEPOINTZERO;
	    winSock.WSACleanup();
	    goto unloadLibrary;
	}

#undef WSA_VERSION_REQUESTED
#undef WSA_VER_MIN_MAJOR
#undef WSA_VER_MIN_MINOR

	/*
	 * Has winsock version 2.x been loaded?
	 */

	if (LOBYTE(winSock.wVersionLoaded) == 2) {

	    /*
	     *  Initialize the 2.x function entries.
	     */

	    winSock.WSAAccept = (LPFN_WSAACCEPT)
		    GetProcAddress(winSock.hModule, "WSAAccept");
	    winSock.WSAAddressToStringA = (LPFN_WSAADDRESSTOSTRINGA)
		    GetProcAddress(winSock.hModule, "WSAAddressToStringA");
	    winSock.WSACloseEvent = (LPFN_WSACLOSEEVENT)
		    GetProcAddress(winSock.hModule, "WSACloseEvent");
	    winSock.WSAConnect = (LPFN_WSACONNECT)
		    GetProcAddress(winSock.hModule, "WSAConnect");
	    winSock.WSACreateEvent = (LPFN_WSACREATEEVENT)
		    GetProcAddress(winSock.hModule, "WSACreateEvent");
	    winSock.WSADuplicateSocketA = (LPFN_WSADUPLICATESOCKETA)
		    GetProcAddress(winSock.hModule, "WSADuplicateSocketA");
	    winSock.WSAEnumNameSpaceProvidersA = (LPFN_WSAENUMNAMESPACEPROVIDERSA)
		    GetProcAddress(winSock.hModule, "WSAEnumNameSpaceProvidersA");
	    winSock.WSAEnumNetworkEvents = (LPFN_WSAENUMNETWORKEVENTS)
		    GetProcAddress(winSock.hModule, "WSAEnumNetworkEvents");
	    winSock.WSAEnumProtocolsA = (LPFN_WSAENUMPROTOCOLSA)
		    GetProcAddress(winSock.hModule, "WSAEnumProtocolsA");
	    winSock.WSAEventSelect = (LPFN_WSAEVENTSELECT)
		    GetProcAddress(winSock.hModule, "WSAEventSelect");
	    winSock.WSAGetOverlappedResult = (LPFN_WSAGETOVERLAPPEDRESULT)
		    GetProcAddress(winSock.hModule, "WSAGetOverlappedResult");
	    winSock.WSAGetQOSByName = (LPFN_WSAGETQOSBYNAME)
		    GetProcAddress(winSock.hModule, "WSAGetQOSByName");
	    winSock.WSAHtonl = (LPFN_WSAHTONL)
		    GetProcAddress(winSock.hModule, "WSAHtonl");
	    winSock.WSAHtons = (LPFN_WSAHTONS)
		    GetProcAddress(winSock.hModule, "WSAHtons");
	    winSock.WSAIoctl = (LPFN_WSAIOCTL)
		    GetProcAddress(winSock.hModule, "WSAIoctl");
	    winSock.WSAJoinLeaf = (LPFN_WSAJOINLEAF)
		    GetProcAddress(winSock.hModule, "WSAJoinLeaf");
	    winSock.WSANtohl = (LPFN_WSANTOHL)
		    GetProcAddress(winSock.hModule, "WSANtohl");
	    winSock.WSANtohs = (LPFN_WSANTOHS)
		    GetProcAddress(winSock.hModule, "WSANtohs");
	    winSock.WSAProviderConfigChange = (LPFN_WSAPROVIDERCONFIGCHANGE)
		    GetProcAddress(winSock.hModule, "WSAProviderConfigChange");
	    winSock.WSARecv = (LPFN_WSARECV)
		    GetProcAddress(winSock.hModule, "WSARecv");
	    winSock.WSARecvDisconnect = (LPFN_WSARECVDISCONNECT)
		    GetProcAddress(winSock.hModule, "WSARecvDisconnect");
	    winSock.WSARecvFrom = (LPFN_WSARECVFROM)
		    GetProcAddress(winSock.hModule, "WSARecvFrom");
	    winSock.WSAResetEvent = (LPFN_WSARESETEVENT)
		    GetProcAddress(winSock.hModule, "WSAResetEvent");
	    winSock.WSASend = (LPFN_WSASEND)
		    GetProcAddress(winSock.hModule, "WSASend");
	    winSock.WSASendDisconnect = (LPFN_WSASENDDISCONNECT)
		    GetProcAddress(winSock.hModule, "WSASendDisconnect");
	    winSock.WSASendTo = (LPFN_WSASENDTO)
		    GetProcAddress(winSock.hModule, "WSASendTo");
	    winSock.WSASetEvent = (LPFN_WSASETEVENT)
		    GetProcAddress(winSock.hModule, "WSASetEvent");
	    winSock.WSASocketA = (LPFN_WSASOCKETA)
		    GetProcAddress(winSock.hModule, "WSASocketA");
	    winSock.WSAStringToAddressA = (LPFN_WSASTRINGTOADDRESSA)
		    GetProcAddress(winSock.hModule, "WSAStringToAddressA");
	    winSock.WSAWaitForMultipleEvents= (LPFN_WSAWAITFORMULTIPLEEVENTS)
		    GetProcAddress(winSock.hModule, "WSAWaitForMultipleEvents");

	    if ((winSock.WSAAccept == NULL) ||
		    (winSock.WSAAddressToStringA == NULL) ||
		    (winSock.WSACloseEvent == NULL) ||
		    (winSock.WSAConnect == NULL) ||
		    (winSock.WSACreateEvent == NULL) ||
		    (winSock.WSADuplicateSocketA == NULL) ||
		    (winSock.WSAEnumNameSpaceProvidersA == NULL) ||
		    (winSock.WSAEnumNetworkEvents == NULL) ||
		    (winSock.WSAEnumProtocolsA == NULL) ||
		    (winSock.WSAEventSelect == NULL) ||
		    (winSock.WSAGetOverlappedResult == NULL) ||
		    (winSock.WSAGetQOSByName == NULL) ||
		    (winSock.WSAHtonl == NULL) ||
		    (winSock.WSAHtons == NULL) ||
		    (winSock.WSAIoctl == NULL) ||
		    (winSock.WSAJoinLeaf == NULL) ||
		    (winSock.WSANtohl == NULL) ||
		    (winSock.WSANtohs == NULL) ||
		    (winSock.WSAProviderConfigChange == NULL) ||
		    (winSock.WSARecv == NULL) ||
		    (winSock.WSARecvDisconnect == NULL) ||
		    (winSock.WSARecvFrom == NULL) ||
		    (winSock.WSAResetEvent == NULL) ||
		    (winSock.WSASend == NULL) ||
		    (winSock.WSASendDisconnect == NULL) ||
		    (winSock.WSASendTo == NULL) ||
		    (winSock.WSASetEvent == NULL) ||
		    (winSock.WSASocketA == NULL) ||
		    (winSock.WSAStringToAddressA == NULL) ||
		    (winSock.WSAWaitForMultipleEvents == NULL)) {
		winsockLoadErr = TCL_WSE_NOTALL2XFUNCS;
		goto unloadLibrary;
	    }

	    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	    GetVersionEx(&os);

	    if (os.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		winsockLoadErr = InitializeIocpSubSystem();
		if (winsockLoadErr != NO_ERROR) {
		    goto unloadLibrary;
		};
	    } else {
		/* InitializeOldSubSystem(); */
		Tcl_Panic("Barf! Can't run IOCP on non-NT systems, sorry.");
	    }
	}
    }

    if (tsdPtr->asyncToken == NULL) {
	Tcl_CreateThreadExitHandler(IocpThreadExitHandler, NULL);
	tsdPtr->asyncToken = Tcl_AsyncCreate(IocpAsyncCallback, NULL);
	tsdPtr->threadId = Tcl_GetCurrentThread();
    }

    return;

unloadLibrary:
    initialized = 0;
    FreeLibrary(winSock.hModule);
    winSock.hModule = NULL;
    return;
}


int
HasSockets(Tcl_Interp *interp)
{
    InitSockets();

    if (winSock.hModule != NULL) {
	return TCL_OK;
    }
    if (interp != NULL) {
	switch (winsockLoadErr) {
	    case TCL_WSE_NOTFOUND:
		Tcl_AppendResult(interp,
			"Windows Sockets are not available on this "
			"system.  A suitable winsock DLL could not be "
			"located.", NULL);
		break;
	    case TCL_WSE_CANTDOONEPOINTZERO:
		Tcl_AppendResult(interp,
			"The Windows Sockets version loaded by "
			"WSAStartup is under 1.1 and is not "
			"accepatable.", NULL);
		break;
	    case TCL_WSE_NOTALL11FUNCS:
		Tcl_AppendResult(interp,
			"The Windows Sockets library didn't have all the"
			" needed exports for the 1.1 interface.", NULL);
		break;
	    case TCL_WSE_NOTALL2XFUNCS:
		Tcl_AppendResult(interp,
			"The Windows Sockets library didn't have all the"
			" needed exports for the 2.x interface.", NULL);
		break;
	    case TCL_WSE_CANTSTARTHANDLERTHREAD:
		Tcl_AppendResult(interp,
			"The Windows Sockets completion thread(s) "
			"were unable to start.", NULL);
		break;
	    default:
		Tcl_AppendObjToObj(Tcl_GetObjResult(interp),
			GetSysMsgObj(winsockLoadErr));
		break;
	}
    }
    return TCL_ERROR;
}

static DWORD
InitializeIocpSubSystem ()
{
    DWORD i, error = NO_ERROR;
    SYSTEM_INFO si;

    GetSystemInfo(&si);
    /* Create the completion port. */
    IocpSubSystem.port = CreateIoCompletionPort(
	    INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)NULL, 0);
    if (IocpSubSystem.port == NULL) {
	error = GetLastError();
	goto done;
    }

    /* Create the private memory heap. */
    IocpSubSystem.heap = HeapCreate(0, (si.dwPageSize*128), 0);
    if (IocpSubSystem.heap == NULL) {
	error = GetLastError();
	CloseHandle(IocpSubSystem.port);
	goto done;
    }

    /* Find out how many processors are on this system. */
    numCPUs = si.dwNumberOfProcessors;
    if (numCPUs > MAX_COMPLETION_THREAD_COUNT) {
	numCPUs = MAX_COMPLETION_THREAD_COUNT;
    }



    /* HACK! HACK! HACK!   AVOID OUT-OF-ORDER PROBLEMS FOR NOW */
    numCPUs = 1;



    /* Start the completion threads -- one per cpu. */
    for(i = 0; i < numCPUs; i++) {
	IocpSubSystem.threads[i] =
		CreateThread(NULL, 0, CompletionThread,
		&IocpSubSystem, 0, NULL);
	if (IocpSubSystem.threads[i] == NULL) {
	    error = TCL_WSE_CANTSTARTHANDLERTHREAD;
	    CloseHandle(IocpSubSystem.port);
	    goto done;
	}
//	SetThreadIdealProcessor(IocpSubSystem.threads[i], i);
//	SetThreadPriority(IocpSubSystem.threads[i],
//		THREAD_PRIORITY_BELOW_NORMAL);
    }
    Tcl_CreateExitHandler(IocpExitHandler, NULL);

done:
    return error;
}

void
IocpExitHandler (ClientData clientData)
{
    DWORD i, j, index, waitResult;

    if (initialized) {
	for (i = 0, j = numCPUs; i < numCPUs; i++, j--) {
	    /* Cause one of the waiting threads to exit. */
	    PostQueuedCompletionStatus(IocpSubSystem.port, 0, 0, 0);

	    /* Wait for one to exit from the group. */
	    waitResult = WaitForMultipleObjects(j,
		    IocpSubSystem.threads, FALSE, INFINITE);

	    /* which one exited? */
	    if ((waitResult >= WAIT_OBJECT_0)
		    && (waitResult < WAIT_OBJECT_0 + j)) {
		index = waitResult - WAIT_OBJECT_0;
		CloseHandle(IocpSubSystem.threads[index]);

		/* Shift left the array of threads to subtract the one
		 * just closed. */
		for (; index < (j + 1); index++) {
		    IocpSubSystem.threads[index] =
			    IocpSubSystem.threads[index+1];
		}
	    }
	}

	/* Close the completion port object. */
	CloseHandle(IocpSubSystem.port);

	/* Tear down the private memory heap. */
	HeapDestroy(IocpSubSystem.heap);
	initialized = 0;
	winSock.WSACleanup();
	FreeLibrary(winSock.hModule);
	winSock.hModule = NULL;
    }
}

void
IocpThreadExitHandler (ClientData clientData)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    /* sanity check */
    if (tsdPtr->asyncToken) Tcl_AsyncDelete(tsdPtr->asyncToken);
}

static int
IocpAsyncCallback(
    ClientData clientData,
    Tcl_Interp *interp,
    int code)
{
    return code;
}

static int
IocpEventCallback(
    Tcl_Event *evPtr,		/* Event to service. */
    int flags)			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    SocketInfo *infoPtr = ((SocketEvent *)evPtr)->infoPtr;

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }
    Tcl_NotifyChannel(infoPtr->channel, infoPtr->readyMask);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSocketAddress --
 *
 *	This function initializes a ADDRINFO structure for a host and port.
 *
 * Results:
 *	1 if the host was valid, 0 if the host could not be converted to
 *	an IP address.
 *
 * Side effects:
 *	Fills in the *ADDRINFO structure.
 *
 *----------------------------------------------------------------------
 */

int
CreateSocketAddress(
     CONST char *addr,
     CONST char *port,
     WS2ProtocolData *pdata,
     LPADDRINFO *addrinfo)
{
    ADDRINFO hints;
    LPADDRINFO lphints;
    int result;

    if (pdata != NULL) {
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_flags  = ((addr) ? 0 : AI_PASSIVE);
	hints.ai_family = pdata->af;
	hints.ai_socktype = pdata->type;
	hints.ai_protocol = pdata->protocol;
	lphints = &hints;
    } else {
	lphints = NULL;
    }

    result = getaddrinfo(addr, port, lphints, addrinfo);

    if (result != 0) {
	winSock.WSASetLastError(result);
	return 0;
    }
    return 1;
}

void
FreeSocketAddress(LPADDRINFO addrinfo)
{
    freeaddrinfo(addrinfo);
}


static int
IocpCloseProc(instanceData, interp)
    ClientData instanceData;	/* The socket to close. */
    Tcl_Interp *interp;		/* Unused. */
{
    return 0; /* errorCode */
}

static int
IocpInputProc(instanceData, buf, toRead, errorCodePtr)
    ClientData instanceData;	/* The socket state. */
    char *buf;			/* Where to store data. */
    int toRead;			/* Maximum number of bytes to read. */
    int *errorCodePtr;		/* Where to store error codes. */
{
    return 0; /* bytesRead */
}

static int
IocpOutputProc(instanceData, buf, toWrite, errorCodePtr)
    ClientData instanceData;	/* The socket state. */
    CONST char *buf;		/* Where to get data. */
    int toWrite;		/* Maximum number of bytes to write. */
    int *errorCodePtr;		/* Where to store error codes. */
{
    return 0; /* bytesWritten */
}

static int
IocpSetOptionProc (
    ClientData instanceData,	/* Socket state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL. */
    CONST char *optionName,	/* Name of the option to set. */
    CONST char *value)		/* New value for option. */
{
    SocketInfo *infoPtr;
    SOCKET sock;
    BOOL val = FALSE;
    int boolVar, rtn;

    infoPtr = (SocketInfo *) instanceData;
    sock = infoPtr->socket;

    if (!stricmp(optionName, "-keepalive")) {
	if (Tcl_GetBoolean(interp, value, &boolVar) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (boolVar) val = TRUE;
	rtn = winSock.setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
		(const char *) &val, sizeof(BOOL));
	if (rtn != 0) {
	    if (interp) {
		Tcl_AppendResult(interp, "couldn't set socket option: ",
			GetSysMsg(winSock.WSAGetLastError()), NULL);
	    }
	    return TCL_ERROR;
	}
	return TCL_OK;

    } else if (!stricmp(optionName, "-nagle")) {
	if (Tcl_GetBoolean(interp, value, &boolVar) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!boolVar) val = TRUE;
	rtn = winSock.setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		(const char *) &val, sizeof(BOOL));
	if (rtn != 0) {
	    if (interp) {
		Tcl_AppendResult(interp, "couldn't set socket option: ",
			GetSysMsg(winSock.WSAGetLastError()), NULL);
	    }
	    return TCL_ERROR;
	}
	return TCL_OK;
    }

    return Tcl_BadChannelOption(interp, optionName, "keepalive nagle");
}

static int
IocpGetOptionProc(instanceData, interp, optionName, dsPtr)
    ClientData instanceData;	/* Socket state. */
    Tcl_Interp *interp;         /* For error reporting - can be NULL */
    CONST char *optionName;	/* Name of the option to
				 * retrieve the value for, or
				 * NULL to get all options and
				 * their values. */
    Tcl_DString *dsPtr;		/* Where to store the computed
				 * value; initialized by caller. */
{
    SocketInfo *infoPtr;
    SOCKADDR_IN sockname;
    SOCKADDR_IN peername;
    struct hostent *hostEntPtr;
    SOCKET sock;
    int size = sizeof(SOCKADDR_IN);
    size_t len = 0;
    char buf[TCL_INTEGER_SPACE];

    
    infoPtr = (SocketInfo *) instanceData;
    sock = (int) infoPtr->socket;
    if (optionName != (char *) NULL) {
        len = strlen(optionName);
    }

    if ((len > 1) && (optionName[1] == 'e') &&
	    (strncmp(optionName, "-error", len) == 0)) {
	int optlen;
	DWORD err;
	int ret;
    
	optlen = sizeof(int);
	ret = winSock.getsockopt(sock, SOL_SOCKET, SO_ERROR,
		(char *)&err, &optlen);
	if (ret == SOCKET_ERROR) {
	    err = winSock.WSAGetLastError();
	}
	if (err) {
	    Tcl_DStringAppend(dsPtr, GetSysMsg(err), -1);
	}
	return TCL_OK;
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 'p') &&
                    (strncmp(optionName, "-peername", len) == 0))) {
        if (winSock.getpeername(sock, (LPSOCKADDR) &peername, &size)
                == 0) {
            if (len == 0) {
                Tcl_DStringAppendElement(dsPtr, "-peername");
                Tcl_DStringStartSublist(dsPtr);
            }
            Tcl_DStringAppendElement(dsPtr,
                    winSock.inet_ntoa(peername.sin_addr));

	    if (peername.sin_addr.s_addr == 0) {
	        hostEntPtr = (struct hostent *) NULL;
	    } else {
	        hostEntPtr = winSock.gethostbyaddr(
                    (char *) &(peername.sin_addr), sizeof(peername.sin_addr),
		    AF_INET);
	    }
            if (hostEntPtr != (struct hostent *) NULL) {
                Tcl_DStringAppendElement(dsPtr, hostEntPtr->h_name);
            } else {
                Tcl_DStringAppendElement(dsPtr,
                        winSock.inet_ntoa(peername.sin_addr));
            }
	    TclFormatInt(buf, winSock.ntohs(peername.sin_port));
            Tcl_DStringAppendElement(dsPtr, buf);
            if (len == 0) {
                Tcl_DStringEndSublist(dsPtr);
            } else {
                return TCL_OK;
            }
        } else {
            /*
             * getpeername failed - but if we were asked for all the options
             * (len==0), don't flag an error at that point because it could
             * be an fconfigure request on a server socket. (which have
             * no peer). {copied from unix/tclUnixChan.c}
             */
            if (len) {
                if (interp) {
                    Tcl_AppendResult(interp, "can't get peername: ",
                                     GetSysMsg(winSock.WSAGetLastError()),
                                     (char *) NULL);
                }
                return TCL_ERROR;
            }
        }
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 's') &&
                    (strncmp(optionName, "-sockname", len) == 0))) {
        if (winSock.getsockname(sock, (LPSOCKADDR) &sockname, &size)
                == 0) {
            if (len == 0) {
                Tcl_DStringAppendElement(dsPtr, "-sockname");
                Tcl_DStringStartSublist(dsPtr);
            }
            Tcl_DStringAppendElement(dsPtr,
                    winSock.inet_ntoa(sockname.sin_addr));
	    if (sockname.sin_addr.s_addr == 0) {
	        hostEntPtr = (struct hostent *) NULL;
	    } else {
	        hostEntPtr = winSock.gethostbyaddr(
                    (char *) &(sockname.sin_addr), sizeof(peername.sin_addr),
		    AF_INET);
	    }
            if (hostEntPtr != (struct hostent *) NULL) {
                Tcl_DStringAppendElement(dsPtr, hostEntPtr->h_name);
            } else {
                Tcl_DStringAppendElement(dsPtr,
                        winSock.inet_ntoa(sockname.sin_addr));
            }
            TclFormatInt(buf, winSock.ntohs(sockname.sin_port));
            Tcl_DStringAppendElement(dsPtr, buf);
            if (len == 0) {
                Tcl_DStringEndSublist(dsPtr);
            } else {
                return TCL_OK;
            }
        } else {
	    if (interp) {
		Tcl_AppendResult(interp, "can't get sockname: ",
				 GetSysMsg(winSock.WSAGetLastError()),
				 (char *) NULL);
	    }
	    return TCL_ERROR;
	}
    }

    if (len == 0 || !strncmp(optionName, "-keepalive", len)) {
	int optlen;
	BOOL opt = FALSE;
    
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-keepalive");
        }
	optlen = sizeof(BOOL);
	winSock.getsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt,
		&optlen);
	if (opt) {
	    Tcl_DStringAppendElement(dsPtr, "1");
	} else {
	    Tcl_DStringAppendElement(dsPtr, "0");
	}
	if (len > 0) return TCL_OK;
    }

    if (len == 0 || !strncmp(optionName, "-nagle", len)) {
	int optlen;
	BOOL opt = FALSE;
    
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-nagle");
        }
	optlen = sizeof(BOOL);
	winSock.getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt,
		&optlen);
	if (opt) {
	    Tcl_DStringAppendElement(dsPtr, "0");
	} else {
	    Tcl_DStringAppendElement(dsPtr, "1");
	}
	if (len > 0) return TCL_OK;
    }

    if (len > 0) {
        return Tcl_BadChannelOption(interp, optionName, "peername sockname keepalive nagle");
    }

    return TCL_OK;
}

static void
IocpWatchProc(instanceData, mask)
    ClientData instanceData;	/* The socket state. */
    int mask;			/* Events of interest; an OR-ed
				 * combination of TCL_READABLE,
				 * TCL_WRITABLE and TCL_EXCEPTION. */
{
}

static int
IocpBlockProc(instanceData, mode)
    ClientData	instanceData;	/* The socket to block/un-block. */
    int mode;			/* TCL_MODE_BLOCKING or
                                 * TCL_MODE_NONBLOCKING. */
{
    return 0;
}

static int
IocpGetHandleProc(instanceData, direction, handlePtr)
    ClientData instanceData;	/* The socket state. */
    int direction;		/* Not used. */
    ClientData *handlePtr;	/* Where to store the handle.  */
{
    return TCL_OK;
}


// WARNING!!!  must only be called from the Tcl thread.
SocketInfo *
NewSocketInfo(socket)
    SOCKET socket;
{
    SocketInfo *infoPtr;

    infoPtr = (SocketInfo *) ckalloc(sizeof(SocketInfo));
    infoPtr->socket = socket;
    infoPtr->bClosing = FALSE;
//    infoPtr->flags = 0;
//    infoPtr->watchEvents = 0;
//    infoPtr->readyEvents = 0;
//    infoPtr->selectEvents = 0;
    infoPtr->acceptProc = NULL;
    infoPtr->lastError = 0;
    infoPtr->OutstandingOps = 0;
    infoPtr->IoCountIssued = 0;
    infoPtr->OutOfOrderSends = NULL;
    InitializeCriticalSection(&infoPtr->critSec);

//    WaitForSingleObject(tsdPtr->socketListLock, INFINITE);
//    infoPtr->nextPtr = tsdPtr->socketList;
//    tsdPtr->socketList = infoPtr;
//    SetEvent(tsdPtr->socketListLock);
    
    return infoPtr;
}

void
FreeSocketInfo(SocketInfo *si)
{
    DeleteCriticalSection(&si->critSec);
    ckfree((char *)si);
}

BUFFER_OBJ *
GetBufferObj(SocketInfo *si, size_t buflen)
{
    BUFFER_OBJ *newobj=NULL;

    // Allocate the object
    newobj = (BUFFER_OBJ *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BUFFER_OBJ));
    if (newobj == NULL)
    {
//        fprintf(stderr, "GetBufferObj: HeapAlloc failed: %d\n", GetLastError());
//        ExitProcess(-1);
    }
    // Allocate the buffer
    newobj->buf = (BYTE *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BYTE)*buflen);
    if (newobj->buf == NULL)
    {
//        fprintf(stderr, "GetBufferObj: HeapAlloc failed: %d\n", GetLastError());
//        ExitProcess(-1);
    }
    newobj->buflen = buflen;
    newobj->addrlen = sizeof(newobj->addr);
    return newobj;
}

void
FreeBufferObj(BUFFER_OBJ *obj)
{
    HeapFree(GetProcessHeap(), 0, obj->buf);
    HeapFree(GetProcessHeap(), 0, obj);
}

SocketInfo *
NewAcceptSockInfo(SOCKET s, SocketInfo *si)
{
    SocketInfo *sockobj;

    sockobj = (SocketInfo *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SocketInfo));
    if (sockobj == NULL)
    {
//	Do something logical.
    }

    // Initialize the members
    sockobj->socket = s;
    sockobj->proto = si->proto;
    sockobj->asyncToken = si->asyncToken;
    sockobj->threadId = si->threadId;

    // For TCP we initialize the IO count to one since the AcceptEx is posted
    //    to receive data
    sockobj->IoCountIssued = ((si->proto->protocol == IPPROTO_TCP) ? 1 : 0);

    InitializeCriticalSection(&sockobj->critSec);

    return sockobj;
}


DWORD
PostOverlappedAccept(SocketInfo *si, BUFFER_OBJ *acceptobj)
{
    DWORD bytes;
    int rc;
    unsigned int addr_stoage = sizeof(SOCKADDR_STORAGE) + 16;

    acceptobj->operation = OP_ACCEPT;

    /*
     * Create a ready client socket of the same type for a future
     * incoming connection.
     */
    acceptobj->socket = winSock.WSASocketA(si->proto->af,
	    si->proto->type, si->proto->protocol, NULL, 0,
	    WSA_FLAG_OVERLAPPED);

    if (acceptobj->socket == INVALID_SOCKET) {
	return SOCKET_ERROR;
    }

    /*
     * The buffer can not be smaller than this.
     */
    _ASSERT(acceptobj->buflen >=
	    ((sizeof(SOCKADDR_STORAGE) + 16) * 2));

    /*
     * Use the special function for overlapped accept() that is provided
     * by the LSP for this socket type.
     */
    rc = si->proto->AcceptEx(si->socket, acceptobj->socket,
	    acceptobj->buf, acceptobj->buflen - (addr_stoage * 2),
	    addr_stoage, addr_stoage, &bytes, &acceptobj->ol);

    if (rc == FALSE) {
	DWORD err = winSock.WSAGetLastError();
	if (err != WSA_IO_PENDING) {
	    //GetSysMsg(err);
	    return SOCKET_ERROR;
	}
    }

    /*
     * Increment the outstanding overlapped count for this socket.
     */
    InterlockedIncrement(&si->OutstandingOps);

    return NO_ERROR;
}

DWORD
PostOverlappedRecv(SocketInfo *si, BUFFER_OBJ *recvobj)
{
    WSABUF wbuf;
    DWORD bytes, flags;
    int rc;

    recvobj->operation = OP_READ;
    wbuf.buf = recvobj->buf;
    wbuf.len = recvobj->buflen;
    flags = 0;

    EnterCriticalSection(&si->critSec);

    // Assign the IO order to this receive. This must be performned within
    //    the critical section. The operation of assigning the IO count and posting
    //    the receive cannot be interupted.
    recvobj->IoOrder = si->IoCountIssued;
    si->IoCountIssued++;

    if (si->proto->type == SOCK_STREAM) {
	rc = winSock.WSARecv(si->socket, &wbuf, 1, &bytes, &flags,
		&recvobj->ol, NULL);
    } else {
	rc = winSock.WSARecvFrom(si->socket, &wbuf, 1, &bytes, &flags,
		(SOCKADDR *)&recvobj->addr, &recvobj->addrlen,
		&recvobj->ol, NULL);
    }

    LeaveCriticalSection(&si->critSec);

    if (rc == SOCKET_ERROR) {
        if (winSock.WSAGetLastError() != WSA_IO_PENDING) {
//            dbgprint("PostRecv: WSARecv* failed: %d\n", WSAGetLastError());
            return SOCKET_ERROR;
        }
    }

    // Increment outstanding overlapped operations
    InterlockedIncrement(&si->OutstandingOps);

    return NO_ERROR;
}

DWORD
PostOverlappedSend(SocketInfo *si, BUFFER_OBJ *sendobj)
{
    WSABUF  wbuf;
    DWORD   bytes;
    int     rc;

    sendobj->operation = OP_WRITE;

    wbuf.buf = sendobj->buf;
    wbuf.len = sendobj->buflen;

    EnterCriticalSection(&si->critSec);

    /* Incrmenting the last send issued and issuing the send should not
     * be interuptable. */
    si->LastSendIssued++;

    if (si->proto->type == SOCK_STREAM) {
	rc = winSock.WSASend(si->socket, &wbuf, 1, &bytes, 0,
		&sendobj->ol, NULL);
    } else {
	rc = winSock.WSASendTo(si->socket, &wbuf, 1, &bytes, 0,
                (SOCKADDR *)&sendobj->addr, sendobj->addrlen,
		&sendobj->ol, NULL);
    }

    LeaveCriticalSection(&si->critSec);

    if (rc == SOCKET_ERROR) {
	if (winSock.WSAGetLastError() != WSA_IO_PENDING) {
            return SOCKET_ERROR;
        }
    }

    /* Increment the outstanding operation count. */
    InterlockedIncrement(&si->OutstandingOps);

    return NO_ERROR;
}

void
InsertPendingSend(SocketInfo *si, BUFFER_OBJ *send)
{
    BUFFER_OBJ *ptr, *prev = NULL;


    EnterCriticalSection(&si->critSec);
    send->next = NULL;

    // This loop finds the place to put the send within the list.
    //    The send list is in the same order as the receives were
    //    posted.
    ptr = si->OutOfOrderSends;
    while (ptr) {
	if (send->IoOrder < ptr->IoOrder) {
	    break;
	}
	prev = ptr;
	ptr = ptr->next;
    }
    if (prev == NULL) {
	// Inserting at head
	si->OutOfOrderSends = send;
	send->next = ptr;
    } else {
	// Insertion somewhere in the middle
	prev->next = send;
	send->next = ptr;
    }

    LeaveCriticalSection(&si->critSec);
}

static int
DoSends(SocketInfo *si)
{
    BUFFER_OBJ *sendobj;
    int ret;

    ret = NO_ERROR;
    EnterCriticalSection(&si->critSec);

    sendobj = si->OutOfOrderSends;
    while ((sendobj) && (sendobj->IoOrder == si->LastSendIssued)) {
        if (PostOverlappedSend(si, sendobj) != NO_ERROR) {
            FreeBufferObj(sendobj);
            ret = SOCKET_ERROR;
            break;
        }
        si->OutOfOrderSends = sendobj = sendobj->next;
    }

    LeaveCriticalSection(&si->critSec);
    return ret;
}

/*
 *----------------------------------------------------------------------
 * CompletionThread --
 *
 *	The "main" for the I/O handling thread(s).
 *
 * Results:
 *
 *	None.  Returns when the completion port is sent a completion
 *	notification with a NULL key by the exit handler
 *	(IocpExitHandler).
 *
 * Side effects:
 *
 *	Without direct interaction from Tcl, in-coming accepts will be
 *	accepted and receives, received.  The results of the operations
 *	are posted and tcl will service them when the event loop is
 *	ready to.  Winsock is never left "dangling" on operations.
 *
 *----------------------------------------------------------------------
 */

DWORD WINAPI
CompletionThread (LPVOID lpParam)
{
    CompletionPortInfo *cpinfo = (CompletionPortInfo *)lpParam;
    SocketInfo *sockInfoPtr;
    BUFFER_OBJ *bufobj = NULL;
    OVERLAPPED *lpOverlapped = NULL;
    DWORD BytesTransfered, Flags, Error;
    BOOL  ok;

    for (;;) {
	Error = NO_ERROR;

	ok = GetQueuedCompletionStatus(cpinfo->port,
		&BytesTransfered, (PULONG_PTR)&sockInfoPtr,
		&lpOverlapped, INFINITE);

	if (ok && !sockInfoPtr) {
	    /* A NULL key indicates closure time for this thread. */
	    break;
	}

	/*
	 * Use the pointer to the overlapped structure and derive from it
	 * the top of the parent BUFFER_OBJ structure it sits in.  If the
	 * position of the overlapped structure moves around within the
	 * BUFFER_OBJ structure declaration, this function will _not_ need
	 * to be modified.
	 */

	bufobj = CONTAINING_RECORD(lpOverlapped, BUFFER_OBJ, ol);

	if (!ok) {
	    /*
	     * If GetQueuedCompletionStatus() returned a failure, call
	     * WSAGetOverlappedResult() to translate the error into a
	     * Winsock error code.
	     */

	    ok = winSock.WSAGetOverlappedResult(sockInfoPtr->socket,
		    lpOverlapped, &BytesTransfered, FALSE, &Flags);

	    if (!ok) {
		Error = winSock.WSAGetLastError();
	    }
	}

	/* Go handle the IO operation. */
        HandleIo(sockInfoPtr, bufobj, cpinfo->port, BytesTransfered,
		Error, Flags);
    }
    return 0;
}

void
HandleIo (
    SocketInfo *si,
    BUFFER_OBJ *buf,
    HANDLE CompPort,
    DWORD BytesTransfered,
    DWORD error,
    DWORD flags)
{
    SocketInfo *clientobj = NULL;     // New client object for accepted connections
    BUFFER_OBJ *recvobj=NULL;       // Used to post new receives on accepted connections
    BOOL bCleanupSocket = FALSE;

    if (error != NO_ERROR) {
        /*
	 * An error occured on a TCP socket, free the associated per I/O
	 * buffer and see if there are any more outstanding operations.
	 * If so we must wait until they are complete as well.
         */

        FreeBufferObj(buf);

        if (InterlockedDecrement(&si->OutstandingOps) == 0) {
//            dbgprint("Freeing socket obj in GetOverlappedResult\n");
	    FreeSocketInfo(si);
        }
        return;
    }

    EnterCriticalSection(&si->critSec);
    if (buf->operation == OP_ACCEPT) {
        HANDLE hrc;
        SOCKADDR_STORAGE *LocalSockaddr=NULL, *RemoteSockaddr=NULL;
        int LocalSockaddrLen, RemoteSockaddrLen;

        /*
	 * Get the address information that is specific to this LSP's
	 * decoder.
	 */
        si->proto->GetAcceptExSockaddrs(buf->buf,
		buf->buflen - ((sizeof(SOCKADDR_STORAGE) + 16) * 2),
                sizeof(SOCKADDR_STORAGE) + 16,
		sizeof(SOCKADDR_STORAGE) + 16,
		(SOCKADDR **)&LocalSockaddr, &LocalSockaddrLen,
                (SOCKADDR **)&RemoteSockaddr, &RemoteSockaddrLen);



        /* Get a new SOCKET_OBJ for the new client connection. */
        clientobj = NewAcceptSockInfo(buf->socket, si);

/* TODO: alert Tcl to this new connection. */
	// IocpAlertToTclNewAccept(buf, BytesTransfered, LocalSockaddr, LocalSockaddrLen, RemoteSockaddr, RemoteSockaddrLen);
__asm nop;


        /* Associate the new socket to our completion port. */
        hrc = CreateIoCompletionPort((HANDLE)clientobj->socket, CompPort,
                (ULONG_PTR)clientobj, 0);

        if (hrc == NULL) {
//            fprintf(stderr, "CompletionThread: CreateIoCompletionPort failed: %d\n",
//                    GetLastError());
            return;
        }

        /* Now post a receive on this new connection. */
        recvobj = GetBufferObj(clientobj, 512);

        if (PostOverlappedRecv(clientobj, recvobj) != NO_ERROR) {
            /* If for some reason the send call fails, clean up the connection. */
            FreeBufferObj(recvobj);
            error = SOCKET_ERROR;
        }
        
        /* Post another new AcceptEx() to replace this one that just fired. */
        PostOverlappedAccept(si, buf);

	if (error != NO_ERROR) {
            if (clientobj->OutstandingOps == 0) {
		winSock.closesocket(clientobj->socket);
		clientobj->socket = INVALID_SOCKET;
//                FreeSocketObj(clientobj);
            } else {
                clientobj->bClosing = TRUE;
            }
            error = NO_ERROR;
	}
    } else if ((buf->operation == OP_READ) && (error == NO_ERROR)) {
        if (BytesTransfered > 0) {
	    /*
	     * Receive completed successfully.  Post another.
	     */

/* TODO: alert Tcl about data ready. */


            if (PostOverlappedRecv(si, buf) != NO_ERROR) {
                /* In the event the recv fails, clean up the connection */
                FreeBufferObj(buf);
                error = SOCKET_ERROR;
            }
        } else {
//            dbgprint("Got 0 byte receive\n");

            // Graceful close - the receive returned 0 bytes read
            si->bClosing = TRUE;

            // Free the receive buffer
            FreeBufferObj(buf);

            if (DoSends(si) != NO_ERROR) {
//                dbgprint("0: cleaning up in zero byte handler\n");
                error = SOCKET_ERROR;
            }

            // If this was the last outstanding operation on socket, clean it up
            if ((si->OutstandingOps == 0) && (si->OutOfOrderSends == NULL)) {
//                dbgprint("1: cleaning up in zero byte handler\n");
                bCleanupSocket = TRUE;
            }
        }
    } else if (buf->operation == OP_WRITE) {
        // Update the counters
//        InterlockedExchangeAdd(&gBytesSent, BytesTransfered);
//        InterlockedExchangeAdd(&gBytesSentLast, BytesTransfered);

        FreeBufferObj(buf);

        if (DoSends(si) != NO_ERROR) {
//            dbgprint("Cleaning up inside OP_WRITE handler\n");
            error = SOCKET_ERROR;
        }
    }

    if (error != NO_ERROR) {
        si->bClosing = TRUE;
    }

    //
    // Check to see if socket is closing
    //
    if ((InterlockedDecrement(&si->OutstandingOps) == 0) &&
        (si->bClosing) &&
        (si->OutOfOrderSends == NULL)) {
	bCleanupSocket = TRUE;
    } else {
	if (DoSends(si) != NO_ERROR) {
	    bCleanupSocket = TRUE;
	}
    }

    LeaveCriticalSection(&si->critSec);

    if (bCleanupSocket) {
	winSock.closesocket(si->socket);
	si->socket = INVALID_SOCKET;
	FreeSocketInfo(si);
    }

    return;
}
