/* ----------------------------------------------------------------------
 *
 * iocpsock_lolevel.c --
 *
 *	Think of this file as being win/tclWinSock.c
 *
 * ----------------------------------------------------------------------
 * RCS: @(#) $Id$
 * ----------------------------------------------------------------------
 */

#include "iocpsock.h"

// Use just one completion thread for now.
// TODO: fixme!
#define IOCP_OUT_OF_ORDER_NOT_WORKING	1

// WSARecv and AcceptEx can tell if more are needed immediatly.
#define IOCP_USE_BURST_DETECTION	1

/*
 * The following declare the winsock loading error codes.
 */

#define TCL_WSE_NOTFOUND		1
#define TCL_WSE_CANTDOONEPOINTZERO	2
#define TCL_WSE_NOTALL11FUNCS		3
#define TCL_WSE_NOTALL2XFUNCS		4
#define TCL_WSE_CANTSTARTHANDLERTHREAD	5
static DWORD winsockLoadErr = 0;

/* some globals defined. */
WinsockProcs winSock;
int initialized = 0;
CompletionPortInfo IocpSubSystem;
GUID AcceptExGuid		= WSAID_ACCEPTEX;
GUID GetAcceptExSockaddrsGuid	= WSAID_GETACCEPTEXSOCKADDRS;
GUID ConnectExGuid		= WSAID_CONNECTEX;
GUID DisconnectExGuid		= WSAID_DISCONNECTEX;
GUID TransmitFileGuid		= WSAID_TRANSMITFILE;
Tcl_ThreadDataKey dataKey;

/* file scope globals. */
static DWORD numCPUs;

/* local prototypes */
static void			InitSockets();
static DWORD			InitializeIocpSubSystem();
static Tcl_ExitProc		IocpExitHandler;
static Tcl_ExitProc		IocpThreadExitHandler;
static Tcl_EventSetupProc	IocpEventSetupProc;
static Tcl_EventCheckProc	IocpEventCheckProc;
static Tcl_EventProc		IocpEventProc;
static Tcl_EventDeleteProc	IocpRemovePendingEvents;
static Tcl_EventDeleteProc	IocpRemoveAllPendingEvents;

static Tcl_DriverCloseProc	IocpCloseProc;
static Tcl_DriverInputProc	IocpInputProc;
static Tcl_DriverOutputProc	IocpOutputProc;
static Tcl_DriverSetOptionProc	IocpSetOptionProc;
static Tcl_DriverGetOptionProc	IocpGetOptionProc;
static Tcl_DriverWatchProc	IocpWatchProc;
static Tcl_DriverGetHandleProc	IocpGetHandleProc;
static Tcl_DriverBlockModeProc	IocpBlockProc;

static void	    IocpAlertToTclNewAccept (SocketInfo *infoPtr,
			    SocketInfo *newClient);
static void	    IocpAcceptOne (SocketInfo *infoPtr);
static DWORD	    PostOverlappedSend (SocketInfo *infoPtr,
			    BufferInfo *sendobj);
static DWORD WINAPI CompletionThreadProc (LPVOID lpParam);
static DWORD WINAPI WatchDogThreadProc (LPVOID lpParam);

/*
 * This structure describes the channel type structure for TCP socket
 * based I/O using the native overlapped interface along with completion
 * ports for maximum efficiency so that most operation are done entirely
 * in kernel-mode.
 */

Tcl_ChannelType IocpChannelType = {
    "iocp",		    /* Type name. */
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




/* =================================================================== */
/* ============= Initailization and shutdown procedures ============== */



static void
InitSockets()
{
    WSADATA wsaData;
    OSVERSIONINFO os;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    /* global/once init */
    if (!initialized) {
	initialized = 1;

	srand(clock());

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
	    winSock.WSALookupServiceBeginA = (LPFN_WSALOOKUPSERVICEBEGINA)
		    GetProcAddress(winSock.hModule, "WSALookupServiceBeginA");
	    winSock.WSALookupServiceEnd = (LPFN_WSALOOKUPSERVICEEND)
		    GetProcAddress(winSock.hModule, "WSALookupServiceEnd");
	    winSock.WSALookupServiceNextA = (LPFN_WSALOOKUPSERVICENEXTA)
		    GetProcAddress(winSock.hModule, "WSALookupServiceNextA");
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
		    (winSock.WSALookupServiceBeginA == NULL) ||
		    (winSock.WSALookupServiceEnd == NULL) ||
		    (winSock.WSALookupServiceNextA == NULL) ||
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
	}

	os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&os);

	if (os.dwPlatformId == VER_PLATFORM_WIN32_NT) {
	    winsockLoadErr = InitializeIocpSubSystem();
	} else {
	    // TODO (long-term): put the old WSAAsyncSelect channel driver code in here too.
	    /* winsockLoadErr = InitializeOldSubSystem(); */
	    Tcl_Panic("Barf! Can't run IOCP on non-NT systems, sorry.");
	}
	if (winsockLoadErr != NO_ERROR) {
	    goto unloadLibrary;
	}
    }

    /* per thread init */
    if (tsdPtr->threadId == 0) {
	Tcl_CreateEventSource(IocpEventSetupProc, IocpEventCheckProc, NULL);
	Tcl_CreateThreadExitHandler(IocpThreadExitHandler, tsdPtr);
	tsdPtr->threadId = Tcl_GetCurrentThread();
	tsdPtr->readySockets = IocpLLCreate();
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
#define IOCP_HEAP_START_SIZE	(si.dwPageSize*64)  /* about 256k */
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
    IocpSubSystem.heap = HeapCreate(0, IOCP_HEAP_START_SIZE, 0);
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

#if IOCP_OUT_OF_ORDER_NOT_WORKING
    numCPUs = 1;
#endif

    /* Start the completion threads -- one per cpu. */
    for(i = 0; i < numCPUs; i++) {
	IocpSubSystem.threads[i] =
		CreateThread(NULL, 0, CompletionThreadProc,
		&IocpSubSystem, 0, NULL);
	if (IocpSubSystem.threads[i] == NULL) {
	    error = TCL_WSE_CANTSTARTHANDLERTHREAD;
	    CloseHandle(IocpSubSystem.port);
	    goto done;
	}
    }

    IocpSubSystem.listeningSockets = IocpLLCreate();
#if IOCP_ACCEPT_BUFSIZE > 0
    /* Start the watchdog thread. */
    IocpSubSystem.stop = CreateEvent(NULL, TRUE, FALSE, NULL);
    IocpSubSystem.watchDogThread = CreateThread(NULL, 0,
	    WatchDogThreadProc, IocpSubSystem.stop, 0, NULL);
#endif

    Tcl_CreateExitHandler(IocpExitHandler, NULL);

done:
    return error;
#undef IOCP_HEAP_START_SIZE
}

void
IocpExitHandler (ClientData clientData)
{
    DWORD i, j, index, waitResult;

    if (initialized) {
	for (i = 0, j = numCPUs; i < numCPUs; i++, j--) {
	    /* Cause one of the waiting I/O handler threads to exit. */
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

#if IOCP_ACCEPT_BUFSIZE > 0
	/* Bring down the watchdog thread. */
	SetEvent(IocpSubSystem.stop);
	WaitForSingleObject(IocpSubSystem.watchDogThread, INFINITE);
	CloseHandle(IocpSubSystem.watchDogThread);
	CloseHandle(IocpSubSystem.stop);
#endif

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
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *) clientData;

    Tcl_DeleteEventSource(IocpEventSetupProc, IocpEventCheckProc, NULL);
    if (initialized) {
	IocpLLPopAll(tsdPtr->readySockets, NULL, 0);
	IocpLLDestroy(tsdPtr->readySockets);
	tsdPtr->readySockets = NULL;
    }
}


/* =================================================================== */
/* ==================== Tcl_Event*Proc procedures ==================== */


/*
 *-----------------------------------------------------------------------
 * IocpEventSetupProc --
 *
 *  Happens before the event loop is to wait in the notifier.
 *
 *-----------------------------------------------------------------------
 */
static void
IocpEventSetupProc (
    ClientData clientData,
    int flags)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    Tcl_Time blockTime = {0, 0};

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }

    /*
     * If any ready events exist now, avoid waiting in the notifier.
     * This function call is very inexpensive.
     */

    if (IocpLLIsNotEmpty(tsdPtr->readySockets)) {
	Tcl_SetMaxBlockTime(&blockTime);
    }
}

/*
 *-----------------------------------------------------------------------
 * IocpEventCheckProc --
 *
 *  Happens after the notifier has waited.
 *
 *-----------------------------------------------------------------------
 */
static void
IocpEventCheckProc (
    ClientData clientData,
    int flags)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    SocketInfo *infoPtr;
    SocketEvent *evPtr;
    int evCount;

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }

    /*
     * Do we have any jobs to queue?  Take a snapshot of the count as
     * of now.
     */

    evCount = IocpLLGetCount(tsdPtr->readySockets);

    while (evCount--) {
	infoPtr = IocpLLPopFront(tsdPtr->readySockets, 0, 0);
	if (infoPtr == NULL) break;
	evPtr = (SocketEvent *) ckalloc(sizeof(SocketEvent));
	evPtr->header.proc = IocpEventProc;
	evPtr->infoPtr = infoPtr;
	Tcl_QueueEvent((Tcl_Event *) evPtr, TCL_QUEUE_TAIL);
    }
}

/*
 *-----------------------------------------------------------------------
 * IocpEventProc --
 *
 *  Tcl's event loop is now servicing this.
 *
 *-----------------------------------------------------------------------
 */
static int
IocpEventProc (
    Tcl_Event *evPtr,		/* Event to service. */
    int flags)			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    SocketInfo *infoPtr = ((SocketEvent *)evPtr)->infoPtr;
    int readyMask = 0;

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }

    if (infoPtr->channel == NULL) {
	/* The event became stale.  How did we get here? */
	return 1;
    }

    /*
     * If an accept is ready, pop just one.  We can't pop all of them
     * or it would be greedy to the event loop and cause the granularity
     * to increase -- which is bad and holds back Tk from doing redraws
     * should a listening socket be in a situation where work is coming
     * in faster than Tcl is able to service it.
     */

    if (infoPtr->readyAccepts != NULL
	    && IocpLLIsNotEmpty(infoPtr->readyAccepts)) {
	IocpAcceptOne(infoPtr);
	return 1;
    }

    if (infoPtr->watchMask & TCL_READABLE && IocpLLIsNotEmpty(infoPtr->llPendingRecv)) {
	readyMask |= TCL_READABLE;
    }

    if (infoPtr->watchMask & TCL_WRITABLE && infoPtr->readyMask & TCL_WRITABLE) {
	readyMask |= TCL_WRITABLE;
	infoPtr->readyMask &= ~(TCL_WRITABLE);
    }

    if (readyMask) Tcl_NotifyChannel(infoPtr->channel, readyMask);
    return 1;
}

void
IocpAcceptOne (SocketInfo *infoPtr)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    char channelName[16 + TCL_INTEGER_SPACE];
    IocpAcceptInfo *acptInfo;

    acptInfo = IocpLLPopFront(infoPtr->readyAccepts, IOCP_LL_NODESTROY, 0);

    /* if the actual count doesn't lineup with the notices, don't barf. */
    if (acptInfo == NULL) {
	return;
    }

    /*
     * Socket handles can be reused at the winsock level, but can be
     * reused before the generic layer closes.  Hide the socket handle
     * just in case we hit this issue.
     */
    wsprintfA(channelName, "iocp%d", (int) acptInfo->clientInfo->socket + (rand()*10000 + (rand()*10)));
    acptInfo->clientInfo->channel = Tcl_CreateChannel(&IocpChannelType, channelName,
	    (ClientData) acptInfo->clientInfo, (TCL_READABLE | TCL_WRITABLE));
    if (Tcl_SetChannelOption(NULL, acptInfo->clientInfo->channel, "-translation",
	    "auto crlf") == TCL_ERROR) {
	Tcl_Close((Tcl_Interp *) NULL, acptInfo->clientInfo->channel);
	return;
    }
    if (Tcl_SetChannelOption(NULL, acptInfo->clientInfo->channel, "-eofchar", "")
	    == TCL_ERROR) {
	Tcl_Close((Tcl_Interp *) NULL, acptInfo->clientInfo->channel);
	return;
    }

    // Had to add this!
    if (Tcl_SetChannelOption(NULL, acptInfo->clientInfo->channel, "-blocking", "0")
	    == TCL_ERROR) {
	Tcl_Close((Tcl_Interp *) NULL, acptInfo->clientInfo->channel);
	return;
    }

    /* The client socket is connected, therefore it is writable. */
    acptInfo->clientInfo->readyMask |= TCL_WRITABLE;
    IocpLLPushBack(tsdPtr->readySockets, acptInfo->clientInfo, NULL);

    /*
     * Invoke the accept callback procedure.
     *
     * TODO: move this to the protocol specific files so the type cast,
     * conversion and invokation signature are proper for the address
     * type.
     */

    if (infoPtr->acceptProc != NULL) {
	(infoPtr->acceptProc) (infoPtr->acceptProcData,
		acptInfo->clientInfo->channel,
		winSock.inet_ntoa(((SOCKADDR_IN *)&acptInfo->remote)->sin_addr),
		winSock.ntohs(((SOCKADDR_IN *)&acptInfo->remote)->sin_port));
    }

    IocpFree(acptInfo);
}

static int
IocpRemovePendingEvents (Tcl_Event *ev, ClientData cData)
{
    SocketInfo *infoPtr = (SocketInfo *) cData;
    SocketEvent *sev = (SocketEvent *) ev;

    if (ev->proc == IocpEventProc && sev->infoPtr == infoPtr) {
	return 1;
    } else {
	return 0;
    }
}

static int
IocpRemoveAllPendingEvents (Tcl_Event *ev, ClientData cData)
{
    if (ev->proc == IocpEventProc) {
	return 1;
    } else {
	return 0;
    }
}

/* =================================================================== */
/* ==================== Tcl_Driver*Proc procedures =================== */

static int
IocpCloseProc (
    ClientData instanceData,	/* The socket to close. */
    Tcl_Interp *interp)		/* Unused. */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    int errorCode = 0, err;

    // The core wants to close channels after the exit handler!
    // Our heap is gone!
    if (initialized) {

	/* artificially increment the count. */
	InterlockedIncrement(&infoPtr->OutstandingOps);

	infoPtr->flags |= IOCP_CLOSING;
	infoPtr->channel = NULL;

	if (infoPtr->proto->type == SOCK_STREAM) {
	    err = winSock.WSASendDisconnect(infoPtr->socket, NULL);
	}
	err = winSock.closesocket(infoPtr->socket);
	infoPtr->socket = INVALID_SOCKET;
	if (err == SOCKET_ERROR) {
	    IocpWinConvertWSAError(winSock.WSAGetLastError());
	    errorCode = Tcl_GetErrno();
	}

	/*
	 * Block waiting for all the pending operations to finish before
	 * deleting the SocketInfo structure.  The main intent is to delete
	 * it here and we must wait for all pending operation to complete
	 * to do this.
	 */

	if (InterlockedDecrement(&infoPtr->OutstandingOps) > 0) {
	    WaitForSingleObject(infoPtr->allDone, INFINITE);
	}

	/*
	 * Remove all events queued in the event loop for this socket.
	 * Ie. backwalk the bucket brigade, by goly.
	 */

	Tcl_DeleteEvents(IocpRemovePendingEvents, infoPtr);

	FreeSocketInfo(infoPtr);
    }

    return errorCode;
}

static int
IocpInputProc (
    ClientData instanceData,	/* The socket state. */
    char *buf,			/* Where to store data. */
    int toRead,			/* Maximum number of bytes to read. */
    int *errorCodePtr)		/* Where to store error codes. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    char *bufPos = buf;
    int bytesRead = 0;
    DWORD timeout;
    BufferInfo *bufPtr;

    *errorCodePtr = 0;

    if (!initialized) {
	*errorCodePtr = ENOTCONN;
	return -1;
    }

    if (infoPtr->llPendingRecv == NULL) {
	/* Not connected.  How did we get here? */
	*errorCodePtr = EAGAIN;
	return -1;
    }

    timeout = (infoPtr->flags & IOCP_BLOCKING ? INFINITE : 0);

    /*
     * Merge as much as toRead will allow.
     */

    while ((bufPtr = IocpLLPopFront(infoPtr->llPendingRecv,
	    IOCP_LL_NODESTROY, timeout)) != NULL) {
	if (bytesRead + (int) bufPtr->used > toRead || bufPtr->used == 0) {
	    /* Too large or have EOF.  Push it back on for later. */
	    IocpLLPushFront(infoPtr->llPendingRecv, bufPtr,
		    &bufPtr->node);
	    IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr,
		    NULL);
	    break;
	}
	if (bufPtr->WSAerr != NO_ERROR) {
	    infoPtr->lastError = bufPtr->WSAerr;
	    IocpWinConvertWSAError(bufPtr->WSAerr);
	    *errorCodePtr = Tcl_GetErrno();
	    bytesRead = -1;
	    FreeBufferObj(bufPtr);
	    break;
	} else {
	    memcpy(bufPos, bufPtr->buf, bufPtr->used);
	    bytesRead += bufPtr->used;
	    bufPos += bufPtr->used;
	}
	FreeBufferObj(bufPtr);
	if (infoPtr->flags & IOCP_BLOCKING) break;
    }

    return bytesRead;
}

static int
IocpOutputProc (
    ClientData instanceData,	/* The socket state. */
    CONST char *buf,		/* Where to get data. */
    int toWrite,		/* Maximum number of bytes to write. */
    int *errorCodePtr)		/* Where to store error codes. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    BufferInfo *bufPtr;
    DWORD WSAerr;

    *errorCodePtr = 0;


    if (!initialized) {
	*errorCodePtr = ENOTCONN;
	return -1;
    }

    if (infoPtr->llPendingRecv == NULL) {
	/* Not connected.  How did we get here? */
	*errorCodePtr = EAGAIN;
	return -1;
    }

    /*
     * Check for a background error on the last write.
     */

    if (infoPtr->lastError) {
	IocpWinConvertWSAError(infoPtr->lastError);
	infoPtr->lastError = NO_ERROR;
	goto error;
    }

    bufPtr = GetBufferObj(infoPtr, toWrite);
    memcpy(bufPtr->buf, buf, toWrite);
    WSAerr = PostOverlappedSend(infoPtr, bufPtr);

    if (WSAerr != NO_ERROR) {
	FreeBufferObj(bufPtr);
	infoPtr->lastError = WSAerr;
	IocpWinConvertWSAError(WSAerr);
	goto error;
    }

    return toWrite;

error:
    *errorCodePtr = Tcl_GetErrno();
    return -1;
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

    if (!initialized) {
	return TCL_OK;
    }

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

// TODO: pass this also to a protocol specific option routine.

    return Tcl_BadChannelOption(interp, optionName, "keepalive nagle");
}

static int
IocpGetOptionProc (
    ClientData instanceData,	/* Socket state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL */
    CONST char *optionName,	/* Name of the option to
				 * retrieve the value for, or
				 * NULL to get all options and
				 * their values. */
    Tcl_DString *dsPtr)		/* Where to store the computed
				 * value; initialized by caller. */
{
    SocketInfo *infoPtr;
    struct hostent *hostEntPtr;
    SOCKET sock;
    int size = sizeof(SOCKADDR_IN);
    size_t len = 0;
    char buf[TCL_INTEGER_SPACE];

    
    if (!initialized) {
	return TCL_OK;
    }

    infoPtr = (SocketInfo *) instanceData;
    sock = (int) infoPtr->socket;
    if (optionName != (char *) NULL) {
        len = strlen(optionName);
    }

    if (len > 1) {
	if ((optionName[1] == 'e') &&
	    (strncmp(optionName, "-error", len) == 0)) {
	    if (infoPtr->lastError != NO_ERROR) {
		IocpWinConvertWSAError(infoPtr->lastError);
		Tcl_DStringAppend(dsPtr, Tcl_ErrnoMsg(Tcl_GetErrno()), -1);
		infoPtr->lastError = NO_ERROR;
	    }
	    return TCL_OK;
#if 0   /* for debugging only */
	} else if ((optionName[1] == 'a') &&
	    (strncmp(optionName, "-acceptors", len) == 0)) {
	    TclFormatInt(buf, infoPtr->llPendingAccepts->lCount);
	    Tcl_DStringAppendElement(dsPtr, buf);
	    return TCL_OK;
#endif
	}
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 'p') &&
                    (strncmp(optionName, "-peername", len) == 0))) {
        if (infoPtr->remoteAddr == NULL) {
	    infoPtr->remoteAddr = IocpAlloc(infoPtr->proto->addrLen);
	    if (winSock.getpeername(sock, infoPtr->remoteAddr, &size) == SOCKET_ERROR) {
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
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-peername");
            Tcl_DStringStartSublist(dsPtr);
        }
        Tcl_DStringAppendElement(dsPtr,
                winSock.inet_ntoa(((LPSOCKADDR_IN)infoPtr->remoteAddr)->sin_addr));

	if (((LPSOCKADDR_IN)infoPtr->remoteAddr)->sin_addr.s_addr == 0) {
	    hostEntPtr = (struct hostent *) NULL;
	} else {
	    hostEntPtr = winSock.gethostbyaddr(
                (char *) &(((LPSOCKADDR_IN)infoPtr->remoteAddr)->sin_addr), sizeof(((LPSOCKADDR_IN)infoPtr->remoteAddr)->sin_addr),
		AF_INET);
	}
        if (hostEntPtr != (struct hostent *) NULL) {
            Tcl_DStringAppendElement(dsPtr, hostEntPtr->h_name);
        } else {
            Tcl_DStringAppendElement(dsPtr,
                    winSock.inet_ntoa(((LPSOCKADDR_IN)infoPtr->remoteAddr)->sin_addr));
        }
	TclFormatInt(buf, winSock.ntohs(((LPSOCKADDR_IN)infoPtr->remoteAddr)->sin_port));
        Tcl_DStringAppendElement(dsPtr, buf);
        if (len == 0) {
            Tcl_DStringEndSublist(dsPtr);
        } else {
            return TCL_OK;
        }
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 's') &&
                    (strncmp(optionName, "-sockname", len) == 0))) {
        if (infoPtr->localAddr == NULL) {
	    infoPtr->localAddr = IocpAlloc(infoPtr->proto->addrLen);
	    if (winSock.getsockname(sock, infoPtr->localAddr, &size) == SOCKET_ERROR) {
		if (interp) {
		    Tcl_AppendResult(interp, "can't get sockname: ",
				     GetSysMsg(winSock.WSAGetLastError()),
				     (char *) NULL);
		}
		return TCL_ERROR;
	    }
	}
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-sockname");
            Tcl_DStringStartSublist(dsPtr);
        }
        Tcl_DStringAppendElement(dsPtr,
                winSock.inet_ntoa(((LPSOCKADDR_IN)infoPtr->localAddr)->sin_addr));
	if (((LPSOCKADDR_IN)infoPtr->localAddr)->sin_addr.s_addr == 0) {
	    hostEntPtr = (struct hostent *) NULL;
	} else {
	    hostEntPtr = winSock.gethostbyaddr(
                (char *) &(((LPSOCKADDR_IN)infoPtr->localAddr)->sin_addr), sizeof(((LPSOCKADDR_IN)infoPtr->localAddr)->sin_addr),
		AF_INET);
	}
        if (hostEntPtr != (struct hostent *) NULL) {
            Tcl_DStringAppendElement(dsPtr, hostEntPtr->h_name);
        } else {
            Tcl_DStringAppendElement(dsPtr,
                    winSock.inet_ntoa(((LPSOCKADDR_IN)infoPtr->localAddr)->sin_addr));
        }
        TclFormatInt(buf, winSock.ntohs(((LPSOCKADDR_IN)infoPtr->localAddr)->sin_port));
        Tcl_DStringAppendElement(dsPtr, buf);
        if (len == 0) {
            Tcl_DStringEndSublist(dsPtr);
        } else {
            return TCL_OK;
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
        return Tcl_BadChannelOption(interp, optionName,
		"peername sockname keepalive nagle");
    }

    return TCL_OK;
}

static void
IocpWatchProc (
    ClientData instanceData,	/* The socket state. */
    int mask)			/* Events of interest; an OR-ed
				 * combination of TCL_READABLE,
				 * TCL_WRITABLE and TCL_EXCEPTION. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    
    if (!initialized) {
	return;
    }

    /*
     * Update the watch events mask. Only if the socket is not a
     * server socket. Fix for SF Tcl Bug #557878.
     */

    if (!infoPtr->acceptProc) {    
        infoPtr->watchMask = 0;
	if (mask & TCL_READABLE) {
	    infoPtr->watchMask |= TCL_READABLE;
	}
	if (mask & TCL_WRITABLE) {
	    Tcl_Time blockTime = { 0, 0 };
	    Tcl_SetMaxBlockTime(&blockTime);
	    infoPtr->watchMask |= TCL_WRITABLE;
	}
    }
}

static int
IocpBlockProc (
    ClientData instanceData,	/* The socket state. */
    int mode)			/* TCL_MODE_BLOCKING or
                                 * TCL_MODE_NONBLOCKING. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;

    if (!initialized) {
	return 0;
    }

    if (mode == TCL_MODE_BLOCKING) {
	infoPtr->flags |= IOCP_BLOCKING;
    } else {
	infoPtr->flags &= ~(IOCP_BLOCKING);
    }
    return 0;
}

static int
IocpGetHandleProc (
    ClientData instanceData,	/* The socket state. */
    int direction,		/* TCL_READABLE or TCL_WRITABLE */
    ClientData *handlePtr)	/* Where to store the handle.  */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;

    if (!initialized) {
	return TCL_OK;
    }

    *handlePtr = (ClientData) infoPtr->socket;
    return TCL_OK;
}

/* =================================================================== */
/* ============== Lo-level buffer and state manipulation ============= */


/* takes buffer ownership */
void
IocpAlertToTclNewAccept (
    SocketInfo *infoPtr,
    SocketInfo *newClient)
{
    IocpAcceptInfo *acptInfo;

    acptInfo = IocpAlloc(sizeof(IocpAcceptInfo));
    if (acptInfo == NULL) {
	return;
    }

    memcpy(&acptInfo->local, newClient->localAddr,
	    newClient->proto->addrLen);
    acptInfo->localLen = newClient->proto->addrLen;
    memcpy(&acptInfo->remote, newClient->remoteAddr,
	    newClient->proto->addrLen);
    acptInfo->remoteLen = newClient->proto->addrLen;
    acptInfo->clientInfo = newClient;

    /*
     * Queue this accept's data into the listening channel's info block.
     */
    IocpLLPushBack(infoPtr->readyAccepts, acptInfo, &acptInfo->node);

    /*
     * Let IocpCheckProc() know this channel has something ready
     * that needs servicing.
     */
    IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr, NULL);

    /* In case Tcl is asleep in the notifier, wake it up. */
    Tcl_ThreadAlert(infoPtr->tsdHome->threadId);
}


SocketInfo *
NewSocketInfo (SOCKET socket)
{
    SocketInfo *infoPtr;

    infoPtr = IocpAlloc(sizeof(SocketInfo));
    infoPtr->channel = NULL;
    infoPtr->socket = socket;
    infoPtr->flags = 0;
    infoPtr->readyAccepts = NULL;  
    infoPtr->acceptProc = NULL;
    infoPtr->localAddr = NULL;	    /* Local sockaddr. */
    infoPtr->remoteAddr = NULL;	    /* Remote sockaddr. */
    
    infoPtr->watchMask = 0;

    infoPtr->lastError = NO_ERROR;
    infoPtr->outstandingSends = 0;
// TODO: need this to be an fconfigure!
    infoPtr->maxOutstandingSends = IOCP_SEND_CONCURRENCY;

    infoPtr->OutstandingOps = 0;
    infoPtr->allDone = CreateEvent(NULL, TRUE, FALSE, NULL); /* manual reset */
    infoPtr->node.ll = NULL;
    return infoPtr;
}

void
FreeSocketInfo (SocketInfo *infoPtr)
{
    BufferInfo *bufPtr;

    if (!infoPtr) return;

    /*
     * Remove all pending ready socket notices that have yet to be queued
     * into Tcl's event loop.
     */
    IocpLLPopAllCompare(infoPtr->tsdHome->readySockets, infoPtr, 0);

    /* Remove ourselves from the global listening list, if we are on it. */
    IocpLLPop(&infoPtr->node, IOCP_LL_NODESTROY);

    /* Just in case... */
    if (infoPtr->socket != INVALID_SOCKET) {
	winSock.closesocket(infoPtr->socket);
    }

    if (infoPtr->localAddr) IocpFree(infoPtr->localAddr);
    if (infoPtr->remoteAddr) IocpFree(infoPtr->remoteAddr);

    if (infoPtr->llPendingAccepts) {
	/*
	 * We can let the list go.  All buffers are (had been) removed by
	 * the completion thread.
	 */
	IocpLLDestroy(infoPtr->llPendingAccepts);
    }
    if (infoPtr->readyAccepts) {
	while ((bufPtr = IocpLLPopFront(infoPtr->readyAccepts,
		IOCP_LL_NODESTROY, 0)) != NULL) {
	    FreeBufferObj(bufPtr);
	}
	IocpLLDestroy(infoPtr->readyAccepts);
    }
    if (infoPtr->llPendingRecv) {
	while ((bufPtr = IocpLLPopFront(infoPtr->llPendingRecv,
		IOCP_LL_NODESTROY, 0)) != NULL) {
	    FreeBufferObj(bufPtr);
	}
	IocpLLDestroy(infoPtr->llPendingRecv);
    }
    CloseHandle(infoPtr->allDone);
    IocpFree(infoPtr);
}

BufferInfo *
GetBufferObj (SocketInfo *infoPtr, SIZE_T buflen)
{
    BufferInfo *bufPtr;

    /* Allocate the object. */
    bufPtr = IocpAlloc(sizeof(BufferInfo));
    if (bufPtr == NULL) {
	return NULL;
    }
    /* Allocate the buffer. */
    bufPtr->buf = IocpAlloc(sizeof(BYTE)*buflen);
    if (bufPtr->buf == NULL) {
	IocpFree(bufPtr);
	return NULL;
    }
    bufPtr->socket = INVALID_SOCKET;
    bufPtr->buflen = buflen;
    bufPtr->addr = NULL;
    bufPtr->WSAerr = NO_ERROR;
    bufPtr->parent = infoPtr;
    bufPtr->node.ll = NULL;
    return bufPtr;
}

void
FreeBufferObj (BufferInfo *bufPtr)
{
    /* Pop itself off any linked-list it may be on. */
    IocpLLPop(&bufPtr->node, IOCP_LL_NODESTROY);
    /* If we have a socket for AcceptEx(), close it. */
    if (bufPtr->socket != INVALID_SOCKET) {
	winSock.closesocket(bufPtr->socket);
    }
    IocpFree(bufPtr->buf);
    IocpFree(bufPtr);
}

SocketInfo *
NewAcceptSockInfo (SOCKET socket, SocketInfo *infoPtr)
{
    SocketInfo *newInfoPtr;

    newInfoPtr = NewSocketInfo(socket);
    if (newInfoPtr == NULL) {
	return NULL;
    }

    /* Initialize some members (partial cloning to the parent). */
    newInfoPtr->proto = infoPtr->proto;
    newInfoPtr->tsdHome = infoPtr->tsdHome;
    newInfoPtr->llPendingRecv = IocpLLCreate();

    return newInfoPtr;
}


DWORD
PostOverlappedAccept (SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    DWORD bytes, WSAerr;
    int rc;
    SIZE_T buflen, addr_storage;

    if (infoPtr->flags & IOCP_CLOSING) return WSAENOTCONN;

    bufPtr->operation = OP_ACCEPT;
    buflen = bufPtr->buflen;
    addr_storage = infoPtr->proto->addrLen + 16;

    /*
     * Create a ready client socket of the same type for a future
     * incoming connection.
     */

    bufPtr->socket = winSock.WSASocketA(infoPtr->proto->af,
	    infoPtr->proto->type, infoPtr->proto->protocol, NULL, 0,
	    WSA_FLAG_OVERLAPPED);

    if (bufPtr->socket == INVALID_SOCKET) {
	return INVALID_SOCKET;
    }

    /*
     * Realloc for the extra needed addr storage space.
     */
    bufPtr->buf = IocpReAlloc(bufPtr->buf, bufPtr->buflen +
	    (addr_storage * 2));
    bufPtr->buflen += (addr_storage * 2);

    /*
     * Increment the outstanding overlapped count for this socket and put
     * the buffer on the pending accepts list.  We need to do this before
     * the operation because it might complete instead of posting.
     */
    InterlockedIncrement(&infoPtr->OutstandingOps);
    IocpLLPushBack(infoPtr->llPendingAccepts, bufPtr, &bufPtr->node);

    /*
     * Use the special function for overlapped accept() that is provided
     * by the LSP of this socket type.
     */

    rc = infoPtr->proto->AcceptEx(infoPtr->socket, bufPtr->socket,
	    bufPtr->buf, bufPtr->buflen - (addr_storage * 2),
	    addr_storage, addr_storage, &bytes, &bufPtr->ol);

    if (rc == FALSE) {
	if ((WSAerr = winSock.WSAGetLastError()) != WSA_IO_PENDING) {
	    InterlockedDecrement(&infoPtr->OutstandingOps);
	    return WSAerr;
	}
    }
#if IOCP_USE_BURST_DETECTION
    else {
	/*
	 * Tested under extreme listening socket abuse it was found that
	 * this logic condition is never met.  AcceptEx _never_ completes
	 * immediatly.  It always returns WSA_IO_PENDING.  Too bad,
	 * as this was looking like a good way to detect and handle burst
	 * conditions.
	 */

	BufferInfo *newBufPtr;

	/*
	 * The AcceptEx() has completed now, and was posted to the port.
	 * Keep giving more AcceptEx() calls to drain the internal
	 * backlog.  Why should we wait for the time when the completion
	 * routine is run if we know the listening socket can take another
	 * right now?  IOW, keep recursing until the WSA_IO_PENDING 
	 * condition is achieved.
	 */

	newBufPtr = GetBufferObj(infoPtr, buflen);
	if (PostOverlappedAccept(infoPtr, newBufPtr) != NO_ERROR) {
	    FreeBufferObj(newBufPtr);
	}
    }
#endif

    return NO_ERROR;
}

DWORD
PostOverlappedRecv (SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    WSABUF wbuf;
    DWORD bytes = 0, flags, WSAerr;
    int rc;

    if (infoPtr->flags & IOCP_CLOSING) return WSAENOTCONN;

    bufPtr->operation = OP_READ;
    wbuf.buf = bufPtr->buf;
    wbuf.len = bufPtr->buflen;
    flags = 0;

    /*
     * Increment the outstanding overlapped count for this socket.
     */
    InterlockedIncrement(&infoPtr->OutstandingOps);

    if (infoPtr->proto->type == SOCK_STREAM) {
	rc = winSock.WSARecv(infoPtr->socket, &wbuf, 1, &bytes, &flags,
		&bufPtr->ol, NULL);
    } else {
	rc = winSock.WSARecvFrom(infoPtr->socket, &wbuf, 1, &bytes, &flags,
		bufPtr->addr, &infoPtr->proto->addrLen,
		&bufPtr->ol, NULL);
    }

    if (rc == SOCKET_ERROR) {
	if ((WSAerr = winSock.WSAGetLastError()) != WSA_IO_PENDING) {
	    InterlockedDecrement(&infoPtr->OutstandingOps);
	    return WSAerr;
	}
    }
#if IOCP_USE_BURST_DETECTION
    else if (bytes > 0) {
	BufferInfo *newBufPtr;

	/*
	 * The WSARecv/From has completed now, and was posted to the port.
	 * Keep giving more WSARecv/From calls to drain the internal
	 * buffer (AFD.sys).  Why should we wait for the time when the
	 * completion routine is run if we know the connected socket can
	 * take another right now?  IOW, keep recursing until the
	 * WSA_IO_PENDING condition is achieved.
	 */

	newBufPtr = GetBufferObj(infoPtr, wbuf.len);
	if (PostOverlappedRecv(infoPtr, newBufPtr) != NO_ERROR) {
	    FreeBufferObj(newBufPtr);
	}
    }
#endif

    return NO_ERROR;
}

DWORD
PostOverlappedSend (SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    WSABUF wbuf;
    DWORD bytes = 0, WSAerr;
    int rc;

    if (infoPtr->flags & IOCP_CLOSING) return WSAENOTCONN;

    bufPtr->operation = OP_WRITE;
    wbuf.buf = bufPtr->buf;
    wbuf.len = bufPtr->buflen;

    /*
     * Increment the outstanding overlapped count for this socket.
     */
    InterlockedIncrement(&infoPtr->OutstandingOps);

    if (infoPtr->proto->type == SOCK_STREAM) {
	rc = winSock.WSASend(infoPtr->socket, &wbuf, 1, &bytes, 0,
		&bufPtr->ol, NULL);
    } else {
	rc = winSock.WSASendTo(infoPtr->socket, &wbuf, 1, &bytes, 0,
                bufPtr->addr, infoPtr->proto->addrLen,
		&bufPtr->ol, NULL);
    }

    if (rc == SOCKET_ERROR) {
	if ((WSAerr = winSock.WSAGetLastError()) != WSA_IO_PENDING) {
	    InterlockedDecrement(&infoPtr->OutstandingOps);
	    return WSAerr;
	}
    } else {
	/*
	 * The WSASend/To completed now, instead of getting posted.
	 * Zap an extra TCL_WRITABLE event to push the send buffers in
	 * winsock until we get them posting and keep them full.  This
	 * seems to be greedy if the peer is on localhost.
	 *
	 * TODO: study this problem.  We need to put a cap on the
	 * concurency allowed.  Sounds like an fconfigure is needed.
	 * The logic below isn't thread-safe, but that's ok as perfection
	 * isn't required.
	 */
	if (infoPtr->outstandingSends
		< infoPtr->maxOutstandingSends) {
	    infoPtr->outstandingSends++;
	    IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr, NULL);
	}
    }

    return NO_ERROR;
}


/* =================================================================== */
/* ================== Lo-level Completion handler ==================== */


/*
 *----------------------------------------------------------------------
 * CompletionThread --
 *
 *	The "main" for the I/O handling thread(s).  One thread per CPU.
 *
 * Results:
 *
 *	None.  Returns when the completion port is sent a completion
 *	notification with a NULL key by the exit handler
 *	(IocpExitHandler).
 *
 * Side effects:
 *
 *	Without direct interaction from Tcl, incoming accepts will be
 *	accepted and receives, received.  The results of the operations
 *	are posted and tcl will service them when the event loop is
 *	ready to.  Winsock is never left "dangling" on operations.
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
CompletionThreadProc (LPVOID lpParam)
{
    CompletionPortInfo *cpinfo = (CompletionPortInfo *)lpParam;
    SocketInfo *infoPtr;
    BufferInfo *bufPtr;
    OVERLAPPED *ol;
    DWORD bytes, flags, WSAerr;
    BOOL ok;

    for (;;) {
	WSAerr = NO_ERROR;
	flags = 0;

	ok = GetQueuedCompletionStatus(cpinfo->port, &bytes,
		(PULONG_PTR) &infoPtr, &ol, INFINITE);

	if (ok && !infoPtr) {
	    /* A NULL key indicates closure time for this thread. */
	    break;
	}

	/*
	 * Use the pointer to the overlapped structure and derive from it
	 * the top of the parent BufferInfo structure it sits in.  If the
	 * position of the overlapped structure moves around within the
	 * BufferInfo structure declaration, this logic does _not_ need
	 * to be modified.
	 */

	bufPtr = CONTAINING_RECORD(ol, BufferInfo, ol);

	if (!ok) {
	    /*
	     * If GetQueuedCompletionStatus() returned a failure on
	     * the operation, call WSAGetOverlappedResult() to
	     * translate the error into a Winsock error code.
	     */

	    ok = winSock.WSAGetOverlappedResult(infoPtr->socket,
		    ol, &bytes, FALSE, &flags);

	    if (!ok) {
		WSAerr = winSock.WSAGetLastError();
	    }
	}

	/* Go handle the IO operation. */
        HandleIo(infoPtr, bufPtr, cpinfo->port, bytes, WSAerr, flags);
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 * HandleIo --
 *
 *	Has all the logic for what to do with a socket "event".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Either deletes the buffer handed to it, or processes it.
 *
 *----------------------------------------------------------------------
 */

static void
HandleIo (
    SocketInfo *infoPtr,
    BufferInfo *bufPtr,
    HANDLE CompPort,
    DWORD bytes,
    DWORD WSAerr,
    DWORD flags)
{
    SocketInfo *newInfoPtr;
    BufferInfo *newBufPtr;
    int i;
    SOCKADDR *local, *remote;
    SIZE_T addr_storage;
    int localLen, remoteLen;


    if (WSAerr == WSA_OPERATION_ABORTED 
	    && infoPtr->flags & IOCP_CLOSING) {
	/* Reclaim cancelled overlapped buffer objects. */
        FreeBufferObj(bufPtr);
	goto done;
    }

    bufPtr->used = bytes;
    /* An error stored in the buffer object takes precedence. */
    if (bufPtr->WSAerr == NO_ERROR) {
	bufPtr->WSAerr = WSAerr;
    }

    switch (bufPtr->operation) {
    case OP_ACCEPT:
	/*
	 * Remove this from the 'pending accepts' list of the
	 * listening socket.  It isn't pending anymore.
	 */
	IocpLLPop(&bufPtr->node, IOCP_LL_NODESTROY);

	if (bufPtr->WSAerr == NO_ERROR) {
	    addr_storage = infoPtr->proto->addrLen + 16;

	    /*
	     * Get the address information from the decoder routine
	     * specific to this socket's LSP.
	     */

	    infoPtr->proto->GetAcceptExSockaddrs(bufPtr->buf,
		    bufPtr->buflen - (addr_storage * 2), addr_storage,
		    addr_storage, &local, &localLen, &remote,
		    &remoteLen);

	    winSock.setsockopt(bufPtr->socket, SOL_SOCKET,
		    SO_UPDATE_ACCEPT_CONTEXT, (char *)&infoPtr->socket,
		    sizeof(SOCKET));

	    /*
	     * Get a new SocketInfo for the new client connection.
	     */

	    newInfoPtr = NewAcceptSockInfo(bufPtr->socket, infoPtr);

	    /*
	     * Set the socket to invalid in the buffer so it won't be
	     * closed when the buffer is reclaimed.
	     */

	    bufPtr->socket = INVALID_SOCKET;

	    /*
	     * Save the remote and local SOCKADDRs to its SocketInfo
	     * struct.
	     */

	    newInfoPtr->localAddr = IocpAlloc(localLen);
	    memcpy(newInfoPtr->localAddr, local, localLen);
	    newInfoPtr->remoteAddr = IocpAlloc(remoteLen);
	    memcpy(newInfoPtr->remoteAddr, remote, remoteLen);

	    /* Alert Tcl to this new connection. */
	    IocpAlertToTclNewAccept(infoPtr, newInfoPtr);

	    /* Associate the new socket to our completion port. */
	    CreateIoCompletionPort((HANDLE) newInfoPtr->socket, CompPort,
		    (ULONG_PTR) newInfoPtr, 0);

	    if (bytes > 0) {
		/* (takes buffer ownership) */
		IocpLLPushBack(newInfoPtr->llPendingRecv, bufPtr,
			&bufPtr->node);

		/*
		 * Let IocpCheckProc() know this new channel has a ready
		 * read that needs servicing.
		 */
		IocpLLPushBack(newInfoPtr->tsdHome->readySockets,
			newInfoPtr, NULL);

		/* Should the notifier be asleep, zap it awake. */
		Tcl_ThreadAlert(newInfoPtr->tsdHome->threadId);
	    } else {
		/* No data received from AcceptEx(). */
		FreeBufferObj(bufPtr);
	    }

	    /* post IOCP_RECV_COUNT recvs. */
	    for(i=0; i < IOCP_RECV_COUNT ;i++) {
		newBufPtr = GetBufferObj(newInfoPtr, IOCP_RECV_BUFSIZE);
		if (PostOverlappedRecv(newInfoPtr, newBufPtr)
			!= NO_ERROR) {
		    /* Oh no, the WSARecv failed. */
		    FreeBufferObj(newBufPtr);
		    break;
		}
	    }

	} else {
	    /* WSA_OPERATION_ABORTED appears to be the only error ever. */
	    FreeBufferObj(bufPtr);
	}
    
	/*
	 * Post another new AcceptEx() to replace this one that just
	 * fired.
	 */

	newBufPtr = GetBufferObj(infoPtr, IOCP_ACCEPT_BUFSIZE);
	if (PostOverlappedAccept(infoPtr, newBufPtr) != NO_ERROR) {
	    /* Oh no, the AcceptEx failed. */
	    FreeBufferObj(newBufPtr);
	}
	break;

    case OP_READ:

	/* (takes buffer ownership) */
	IocpLLPushBack(infoPtr->llPendingRecv, bufPtr, &bufPtr->node);

	/*
	 * Let IocpCheckProc() know this channel has a ready read
	 * that needs servicing.
	 */
	IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr, NULL);

	/* Should the notifier be asleep, zap it awake. */
	Tcl_ThreadAlert(infoPtr->tsdHome->threadId);

	if (bytes > 0) {
	    /*
	     * Create a new buffer object, but use a hard-coded size for
	     * now until a method to control the receive buffer size
	     * exists.
	     *
	     * TODO: make an fconfigure for this and store it in the
	     * SocketInfo struct.
	     */

	    newBufPtr = GetBufferObj(infoPtr, IOCP_RECV_BUFSIZE);
	    if (PostOverlappedRecv(infoPtr, newBufPtr) != NO_ERROR) {
		/* Oh no, the WSARecv failed. */
		FreeBufferObj(newBufPtr);
	    }
	}
	break;

    case OP_WRITE:

	infoPtr->lastError = bufPtr->WSAerr;
	FreeBufferObj(bufPtr);

	if (infoPtr->outstandingSends > 0) {
	    infoPtr->outstandingSends--;
	}
	infoPtr->readyMask |= TCL_WRITABLE;

	/*
	 * Let IocpCheckProc() know this channel is writable again.
	 */
	IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr, NULL);

	/* Should the notifier be asleep, zap it awake. */
	Tcl_ThreadAlert(infoPtr->tsdHome->threadId);
	break;

    case OP_CONNECT:

	infoPtr->readyMask |= TCL_WRITABLE;

	if (bufPtr->WSAerr != NO_ERROR) {
	    infoPtr->lastError = bufPtr->WSAerr;
	} else {
	    infoPtr->llPendingRecv = IocpLLCreate();

	    winSock.setsockopt(infoPtr->socket, SOL_SOCKET,
		    SO_UPDATE_CONNECT_CONTEXT, NULL, 0);

	    /* post IOCP_RECV_COUNT recvs. */
	    for(i=0; i < IOCP_RECV_COUNT ;i++) {
		newBufPtr = GetBufferObj(infoPtr, IOCP_RECV_BUFSIZE);
		if (PostOverlappedRecv(infoPtr, newBufPtr) != NO_ERROR) {
		    /* Oh no, the WSARecv failed. */
		    FreeBufferObj(newBufPtr);
		    break;
		}
	    }
	}
	FreeBufferObj(bufPtr);

	/*
	 * Let IocpCheckProc() know this channel is writable.
	 */
	IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr,	NULL);

	/* Should the notifier be asleep, zap it awake. */
	Tcl_ThreadAlert(infoPtr->tsdHome->threadId);
	break;

    case OP_DISCONNECT:
    case OP_TRANSMIT:
    case OP_LOOKUP:
	/* For future use. */
	break;
    }

done:
    if (InterlockedDecrement(&infoPtr->OutstandingOps) <= 0
	    && infoPtr->flags & IOCP_CLOSING) {
	/* This is the last operation. */
	SetEvent(infoPtr->allDone);
    }
}

/*
 *----------------------------------------------------------------------
 * WatchDogThreadProc --
 *
 *	This prevents denial-of-service attacks by cleaning-up accepted
 *	sockets that have not received data yet.  AcceptEx can be
 *	optimized to only fire after data is received on the new
 *	connection. IOW, the peer must talk first.  This only true when
 *	PostOverlappedAccept() is called with a buffer size greater than
 *	zero to indicate a receive is desired too.
 *
 * Results:
 *
 *	None.
 *
 * Side effects:
 *
 *	Can close a socket on a listener's llPendingAccepts list.
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
WatchDogThreadProc (LPVOID lpParam)
{
    HANDLE hStop = (HANDLE) lpParam;
    DWORD dwWait;
    SocketInfo *infoPtr, *nextInfoPtr;
    BufferInfo *bufPtr;

    for (;;) {
	/* 20 second timeout. */
        dwWait = WaitForSingleObject(hStop, 20000);

        if (dwWait == WAIT_OBJECT_0 || dwWait == WAIT_FAILED) {
	    /* The wait succeded (or errored), therefore we close. */
            break;

        } else if (dwWait == WAIT_TIMEOUT) {
	    int optval, optlen, code;

	    /* Get the first entry on the listening list. */
            infoPtr = (IocpSubSystem.listeningSockets->front ? IocpSubSystem.listeningSockets->front->lpItem : NULL);
            while (infoPtr) {
		bufPtr = (infoPtr->llPendingAccepts->front ? infoPtr->llPendingAccepts->front->lpItem : NULL);
                while (bufPtr) {
                    optlen = sizeof(optval);
                    code = winSock.getsockopt(bufPtr->socket, SOL_SOCKET,
			    SO_CONNECT_TIME, (char *)&optval, &optlen);
                    if (code == SOCKET_ERROR) {
			/* Ignore the error, continue. */
			bufPtr = bufPtr->node.next->lpItem;
                        continue;
                    }
                    /*
		     * If the socket has been connected for more than 2 minutes,
		     * close it. If closed, the AcceptEx call will fail in the
		     * completion thread and get replaced with a new one.
		     */
		    if (optval != 0xFFFFFFFF && optval > 120) {
			EnterCriticalSection(&infoPtr->llPendingAccepts->lock);
			winSock.closesocket(bufPtr->socket);
			bufPtr->socket = INVALID_SOCKET;
			LeaveCriticalSection(&infoPtr->llPendingAccepts->lock);
		    }
		    bufPtr = (bufPtr->node.next ? bufPtr->node.next->lpItem : NULL);
		}
		nextInfoPtr = (infoPtr->node.next ? infoPtr->node.next->lpItem : NULL);
		infoPtr = nextInfoPtr;
            }
	}
    }

    return 0;
}

/* =================================================================== */
/* ====================== Private memory heap ======================== */

__inline LPVOID
IocpAlloc (SIZE_T size)
{
    LPVOID p;
    p = HeapAlloc(IocpSubSystem.heap, HEAP_ZERO_MEMORY, size);
    return p;
}

__inline LPVOID
IocpReAlloc (LPVOID block, SIZE_T size)
{
    LPVOID p;
    p = HeapReAlloc(IocpSubSystem.heap, HEAP_ZERO_MEMORY, block, size);
    return p;
}


__inline BOOL
IocpFree (LPVOID block)
{
    return HeapFree(IocpSubSystem.heap, 0, block);
}

/* =================================================================== */
/* ========================= Linked-List ============================= */

/* Bitmask macros. */
#define mask_a( mask, val ) if ( ( mask & val ) != val ) { mask |= val; }
#define mask_d( mask, val ) if ( ( mask & val ) == val ) { mask &= ~(val); }
#define mask_y( mask, val ) ( mask & val ) == val
#define mask_n( mask, val ) ( mask & val ) != val


/*
 *----------------------------------------------------------------------
 *
 * IocpLLCreate --
 *
 *	Creates a linked list.
 *
 * Results:
 *	pointer to the new one or NULL for error.
 *
 * Side effects:
 *	None known.
 *
 *----------------------------------------------------------------------
 */

LPLLIST
IocpLLCreate ()
{   
    LPLLIST ll;
    
    /* Alloc a linked list. */
    ll = IocpAlloc(sizeof(LLIST));
    if (!ll) {
	return NULL;
    }
    if (!InitializeCriticalSectionAndSpinCount(&ll->lock, 4000)) {
	IocpFree(ll);
	return NULL;
    }
    ll->lCount = 0;
    return ll;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLDestroy --
 *
 *	Destroys a linked list.
 *
 * Results:
 *	Same as HeapFree.
 *
 * Side effects:
 *	Nodes aren't destroyed.
 *
 *----------------------------------------------------------------------
 */

BOOL 
IocpLLDestroy (
    LPLLIST ll)
{
    if (!ll) {
	return FALSE;
    }
    DeleteCriticalSection(&ll->lock);
    return IocpFree(ll);
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPushFront --
 *
 *	Adds an item to the end of the list.
 *
 * Results:
 *	The node.
 *
 * Side effects:
 *	Will create a new node, if not given one.
 *
 *----------------------------------------------------------------------
 */

LPLLNODE 
IocpLLPushBack(
    LPLLIST ll,
    LPVOID lpItem,
    LPLLNODE pnode)
{
    LPLLNODE tmp;

    if (!ll) {
	return NULL;
    }
    EnterCriticalSection(&ll->lock);
    if (!pnode) {
	pnode = IocpAlloc(sizeof(LLNODE));
    }
    if (!pnode) {
	LeaveCriticalSection(&ll->lock);
	return NULL;
    }
    pnode->lpItem = lpItem;
    if (!ll->front && !ll->back) {
	ll->front = pnode;
	ll->back = pnode;
    } else {
	ll->back->next = pnode;
	tmp = ll->back;
	ll->back = pnode;
	ll->back->prev = tmp;
    }
    ll->lCount++;
    pnode->ll = ll;
    LeaveCriticalSection(&ll->lock);
    return pnode;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPushFront --
 *
 *	Adds an item to the front of the list.
 *
 * Results:
 *	The node.
 *
 * Side effects:
 *	Will create a new node, if not given one.
 *
 *----------------------------------------------------------------------
 */

LPLLNODE 
IocpLLPushFront(
    LPLLIST ll,
    LPVOID lpItem,
    LPLLNODE pnode)
{
    LPLLNODE tmp;

    if (!ll) {
	return NULL;
    }
    EnterCriticalSection(&ll->lock);
    if (!pnode) {
	pnode = IocpAlloc(sizeof(LLNODE));
    }
    if (!pnode) {
	LeaveCriticalSection(&ll->lock);
	return NULL;
    }
    pnode->lpItem = lpItem;
    if (!ll->front && !ll->back) {
	ll->front = pnode;
	ll->back = pnode;
    } else {
	ll->front->prev = pnode;
	tmp = ll->front;
	ll->front = pnode;
	ll->front->next = tmp;
    }
    ll->lCount++;
    pnode->ll = ll;
    LeaveCriticalSection(&ll->lock);
    return pnode;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPopAll --
 *
 *	Removes all items from the list.
 *
 * Results:
 *	TRUE if something was popped or FALSE if nothing was poppable.
 *
 * Side effects:
 *	Won't free the node(s) with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

BOOL 
IocpLLPopAll(
    LPLLIST ll,
    LPLLNODE snode,
    DWORD dwState)
{
    LPLLNODE tmp1, tmp2;

    if (!ll) {
	return FALSE;
    }
    if (snode && snode->ll) {
	ll = snode->ll;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && ! ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return FALSE;
    }
    tmp1 = ll->front;
    if (snode) {
	tmp1 = snode;
    }
    while(tmp1) {
	tmp2 = tmp1->next;
	/* Delete (or blank) the node and decrement the counter. */
        if (mask_n(dwState, IOCP_LL_NODESTROY)) {
	    IocpLLNodeDestroy(tmp1);
	} else {
	    tmp1->ll = NULL;
	    tmp1->next = NULL; 
            tmp1->prev = NULL;
	}
        ll->lCount--;
	tmp1 = tmp2;
    }

    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    
    return TRUE;
}


BOOL 
IocpLLPopAllCompare(
    LPLLIST ll,
    LPVOID lpItem,
    DWORD dwState)
{
    LPLLNODE tmp1, tmp2;

    if (!ll) {
	return FALSE;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && !ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return FALSE;
    }
    tmp1 = ll->front;
    while(tmp1) {
	tmp2 = tmp1->next;
	if (tmp1->lpItem == lpItem) {
	    IocpLLPop(tmp1, IOCP_LL_NOLOCK | dwState);
	}
	tmp1 = tmp2;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPop --
 *
 *	Removes an item from the list.
 *
 * Results:
 *	TRUE if something was popped or FALSE if nothing was poppable.
 *
 * Side effects:
 *	Won't free the node with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

BOOL 
IocpLLPop(
    LPLLNODE node,
    DWORD dwState)
{
    LPLLIST ll;
    LPLLNODE prev, next;

    //Ready the node
    if (!node || !node->ll) {
	return FALSE;
    }
    ll = node->ll;
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && !ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return FALSE;
    }
    prev = node->prev;
    next = node->next;

    /* Check for only item. */
    if (!prev & !next) {
	ll->front = NULL;
	ll->back = NULL;
    /* Check for front of list. */
    } else if (!prev && next) {
	next->prev = NULL;
	ll->front = next;
    /* Check for back of list. */
    } else if (prev && !next) {
	prev->next = NULL;
	ll->back = prev;
    /* Check for middle of list. */
    } else if (prev && next) {
	next->prev = prev;
	prev->next = next;
    }

    /* Delete the node when IOCP_LL_NODESTROY is not specified. */
    if (mask_n(dwState, IOCP_LL_NODESTROY)) {
	IocpLLNodeDestroy(node);
    } else {
	node->ll = NULL;
	node->next = NULL; 
        node->prev = NULL;
    }
    ll->lCount--;
    if (ll->lCount <= 0) {
	ll->front = NULL;
	ll->back = NULL;
    }

    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLNodeDestroy --
 *
 *	Destroys a node.
 *
 * Results:
 *	Same as HeapFree.
 *
 * Side effects:
 *	memory returns to the system.
 *
 *----------------------------------------------------------------------
 */

BOOL
IocpLLNodeDestroy (LPLLNODE node)
{
    return IocpFree(node);
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPopBack --
 *
 *	Removes the item at the back of the list.
 *
 * Results:
 *	The item stored in the node at the front or NULL for none.
 *
 * Side effects:
 *	Won't free the node with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

LPVOID
IocpLLPopBack(
    LPLLIST ll,
    DWORD dwState,
    DWORD timeout)
{
    LPLLNODE tmp;
    LPVOID vp;

    if (!ll) {
	return NULL;
    }
    EnterCriticalSection(&ll->lock);
    if (!ll->front && !ll->back) {
	LeaveCriticalSection(&ll->lock);
	return NULL;
    }
    tmp = ll->back;
    vp = tmp->lpItem;
    IocpLLPop(tmp, IOCP_LL_NOLOCK | dwState);
    LeaveCriticalSection(&ll->lock);
    return vp;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPopFront --
 *
 *	Removes the item at the front of the list.
 *
 * Results:
 *	The item stored in the node at the front or NULL for none.
 *
 * Side effects:
 *	Won't free the node with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

LPVOID
IocpLLPopFront(
    LPLLIST ll,
    DWORD dwState,
    DWORD timeout)
{
    LPLLNODE tmp;
    LPVOID vp;

    if (!ll) {
	return NULL;
    }
    EnterCriticalSection(&ll->lock);
    if (!ll->lCount) {
	LeaveCriticalSection(&ll->lock);
	return NULL;
    }
    if (!ll->front && !ll->back) {
	LeaveCriticalSection(&ll->lock);
	return NULL;
    }
    tmp = ll->front;
    vp = tmp->lpItem;
    IocpLLPop(tmp, IOCP_LL_NOLOCK | dwState);
    LeaveCriticalSection(&ll->lock);
    return vp;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLIsNotEmpty --
 *
 *	self explanatory.
 *
 * Results:
 *	Boolean for if the linked-list has entries.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

BOOL
IocpLLIsNotEmpty (LPLLIST ll)
{
    BOOL b;
    if (!ll) {
	return FALSE;
    }
    EnterCriticalSection(&ll->lock);
    b = (ll->lCount != 0);
    LeaveCriticalSection(&ll->lock);
    return b;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLGetCount --
 *
 *	How many nodes are on the list?
 *
 * Results:
 *	Count of entries.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

SIZE_T
IocpLLGetCount (LPLLIST ll)
{
    SIZE_T c;
    if (!ll) {
	return 0;
    }
    EnterCriticalSection(&ll->lock);
    c = ll->lCount;
    LeaveCriticalSection(&ll->lock);
    return c;
}


/* =================================================================== */
/* ============== Protocol neutral SOCKADDR procedures =============== */


/*
 *-----------------------------------------------------------------------
 *
 * CreateSocketAddress --
 *
 *	This function initializes a ADDRINFO structure for a host and
 *	port.
 *
 * Results:
 *	1 if the host was valid, 0 if the host could not be converted to
 *	an IP address.
 *
 * Side effects:
 *	Fills in the *ADDRINFO structure.
 *
 *-----------------------------------------------------------------------
 */

int
CreateSocketAddress (
     const char *addr,
     const char *port,
     LPADDRINFO inhints,
     LPADDRINFO *paddrinfo)
{
    ADDRINFO hints;
    LPADDRINFO phints;
    int result;

    if (inhints != NULL) {
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_flags  = ((addr) ? 0 : AI_PASSIVE);
	hints.ai_family = inhints->ai_family;
	hints.ai_socktype = inhints->ai_socktype;
	hints.ai_protocol = inhints->ai_protocol;
	phints = &hints;
    } else {
	phints = NULL;
    }

    result = getaddrinfo(addr, port, phints, paddrinfo);

    if (result != 0) {
//	winSock.WSASetLastError(result);
	return 0;
    }
    return 1;
}

void
FreeSocketAddress (LPADDRINFO addrinfo)
{
    freeaddrinfo(addrinfo);
}

/* =================================================================== */
/* ========================= Error mappings ========================== */

/*
 * The following table contains the mapping from WinSock errors to
 * errno errors.
 */

static int wsaErrorTable1[] = {
    EWOULDBLOCK,	/* WSAEWOULDBLOCK */
    EINPROGRESS,	/* WSAEINPROGRESS */
    EALREADY,		/* WSAEALREADY */
    ENOTSOCK,		/* WSAENOTSOCK */
    EDESTADDRREQ,	/* WSAEDESTADDRREQ */
    EMSGSIZE,		/* WSAEMSGSIZE */
    EPROTOTYPE,		/* WSAEPROTOTYPE */
    ENOPROTOOPT,	/* WSAENOPROTOOPT */
    EPROTONOSUPPORT,	/* WSAEPROTONOSUPPORT */
    ESOCKTNOSUPPORT,	/* WSAESOCKTNOSUPPORT */
    EOPNOTSUPP,		/* WSAEOPNOTSUPP */
    EPFNOSUPPORT,	/* WSAEPFNOSUPPORT */
    EAFNOSUPPORT,	/* WSAEAFNOSUPPORT */
    EADDRINUSE,		/* WSAEADDRINUSE */
    EADDRNOTAVAIL,	/* WSAEADDRNOTAVAIL */
    ENETDOWN,		/* WSAENETDOWN */
    ENETUNREACH,	/* WSAENETUNREACH */
    ENETRESET,		/* WSAENETRESET */
    ECONNABORTED,	/* WSAECONNABORTED */
    ECONNRESET,		/* WSAECONNRESET */
    ENOBUFS,		/* WSAENOBUFS */
    EISCONN,		/* WSAEISCONN */
    ENOTCONN,		/* WSAENOTCONN */
    ESHUTDOWN,		/* WSAESHUTDOWN */
    ETOOMANYREFS,	/* WSAETOOMANYREFS */
    ETIMEDOUT,		/* WSAETIMEDOUT */
    ECONNREFUSED,	/* WSAECONNREFUSED */
    ELOOP,		/* WSAELOOP */
    ENAMETOOLONG,	/* WSAENAMETOOLONG */
    EHOSTDOWN,		/* WSAEHOSTDOWN */
    EHOSTUNREACH,	/* WSAEHOSTUNREACH */
    ENOTEMPTY,		/* WSAENOTEMPTY */
    EAGAIN,		/* WSAEPROCLIM */
    EUSERS,		/* WSAEUSERS */
    EDQUOT,		/* WSAEDQUOT */
    ESTALE,		/* WSAESTALE */
    EREMOTE,		/* WSAEREMOTE */
};

/*
 * These error codes are very windows specific and have no POSIX
 * translation, yet.
 *
 * TODO: Fixme!
 */

static int wsaErrorTable2[] = {
    EINVAL,		/* WSASYSNOTREADY	    WSAStartup cannot function at this time because the underlying system it uses to provide network services is currently unavailable. */
    EINVAL,		/* WSAVERNOTSUPPORTED	    The Windows Sockets version requested is not supported. */
    EINVAL,		/* WSANOTINITIALISED	    Either the application has not called WSAStartup, or WSAStartup failed. */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    EINVAL,		/* WSAEDISCON		    Returned by WSARecv or WSARecvFrom to indicate the remote party has initiated a graceful shutdown sequence. */
    EINVAL,		/* WSAENOMORE		    No more results can be returned by WSALookupServiceNext. */
    EINVAL,		/* WSAECANCELLED	    A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled. */
    EINVAL,		/* WSAEINVALIDPROCTABLE	    The procedure call table is invalid. */
    EINVAL,		/* WSAEINVALIDPROVIDER	    The requested service provider is invalid. */
    EINVAL,		/* WSAEPROVIDERFAILEDINIT   The requested service provider could not be loaded or initialized. */
    EINVAL,		/* WSASYSCALLFAILURE	    A system call that should never fail has failed. */
    EINVAL,		/* WSASERVICE_NOT_FOUND	    No such service is known. The service cannot be found in the specified name space. */
    EINVAL,		/* WSATYPE_NOT_FOUND	    The specified class was not found. */
    EINVAL,		/* WSA_E_NO_MORE	    No more results can be returned by WSALookupServiceNext. */
    EINVAL,		/* WSA_E_CANCELLED	    A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled. */
    EINVAL,		/* WSAEREFUSED		    A database query failed because it was actively refused. */
};

/*
 * These error codes are very windows specific and have no POSIX
 * translation, yet.
 *
 * TODO: Fixme!
 */

static int wsaErrorTable3[] = {
    EINVAL,	/* WSAHOST_NOT_FOUND,	Authoritative Answer: Host not found */
    EINVAL,	/* WSATRY_AGAIN,	Non-Authoritative: Host not found, or SERVERFAIL */
    EINVAL,	/* WSANO_RECOVERY,	Non-recoverable errors, FORMERR, REFUSED, NOTIMP */
    EINVAL,	/* WSANO_DATA,		Valid name, no data record of requested type */
    EINVAL,	/* WSA_QOS_RECEIVERS,		at least one Reserve has arrived */
    EINVAL,	/* WSA_QOS_SENDERS,		at least one Path has arrived */
    EINVAL,	/* WSA_QOS_NO_SENDERS,		there are no senders */
    EINVAL,	/* WSA_QOS_NO_RECEIVERS,	there are no receivers */
    EINVAL,	/* WSA_QOS_REQUEST_CONFIRMED,	Reserve has been confirmed */
    EINVAL,	/* WSA_QOS_ADMISSION_FAILURE,	error due to lack of resources */
    EINVAL,	/* WSA_QOS_POLICY_FAILURE,	rejected for administrative reasons - bad credentials */
    EINVAL,	/* WSA_QOS_BAD_STYLE,		unknown or conflicting style */
    EINVAL,	/* WSA_QOS_BAD_OBJECT,		problem with some part of the filterspec or providerspecific buffer in general */
    EINVAL,	/* WSA_QOS_TRAFFIC_CTRL_ERROR,	problem with some part of the flowspec */
    EINVAL,	/* WSA_QOS_GENERIC_ERROR,	general error */
    EINVAL,	/* WSA_QOS_ESERVICETYPE,	invalid service type in flowspec */
    EINVAL,	/* WSA_QOS_EFLOWSPEC,		invalid flowspec */
    EINVAL,	/* WSA_QOS_EPROVSPECBUF,	invalid provider specific buffer */
    EINVAL,	/* WSA_QOS_EFILTERSTYLE,	invalid filter style */
    EINVAL,	/* WSA_QOS_EFILTERTYPE,		invalid filter type */
    EINVAL,	/* WSA_QOS_EFILTERCOUNT,	incorrect number of filters */
    EINVAL,	/* WSA_QOS_EOBJLENGTH,		invalid object length */
    EINVAL,	/* WSA_QOS_EFLOWCOUNT,		incorrect number of flows */
    EINVAL,	/* WSA_QOS_EUNKOWNPSOBJ,	unknown object in provider specific buffer */
    EINVAL,	/* WSA_QOS_EPOLICYOBJ,		invalid policy object in provider specific buffer */
    EINVAL,	/* WSA_QOS_EFLOWDESC,		invalid flow descriptor in the list */
    EINVAL,	/* WSA_QOS_EPSFLOWSPEC,		inconsistent flow spec in provider specific buffer */
    EINVAL,	/* WSA_QOS_EPSFILTERSPEC,	invalid filter spec in provider specific buffer */
    EINVAL,	/* WSA_QOS_ESDMODEOBJ,		invalid shape discard mode object in provider specific buffer */
    EINVAL,	/* WSA_QOS_ESHAPERATEOBJ,	invalid shaping rate object in provider specific buffer */
    EINVAL,	/* WSA_QOS_RESERVED_PETYPE,	reserved policy element in provider specific buffer */
};

/*
 *----------------------------------------------------------------------
 *
 * IocpWinConvertWSAError --
 *
 *	This routine converts a WinSock error into an errno value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the errno global variable.
 *
 *----------------------------------------------------------------------
 */

void
IocpWinConvertWSAError(
    DWORD errCode)	/* Win32 WSA error code. */		
{
    if ((errCode >= WSAEWOULDBLOCK) && (errCode <= WSAEREMOTE)) {
	Tcl_SetErrno(wsaErrorTable1[errCode - WSAEWOULDBLOCK]);
    } else if ((errCode >= WSASYSNOTREADY) && (errCode <= WSAEREFUSED)) {
	Tcl_SetErrno(wsaErrorTable2[errCode - WSASYSNOTREADY]);
    } else if ((errCode >= WSAHOST_NOT_FOUND) && (errCode <= WSA_QOS_RESERVED_PETYPE)) {
	Tcl_SetErrno(wsaErrorTable3[errCode - WSAHOST_NOT_FOUND]);
    } else {
	Tcl_SetErrno(EINVAL);
    }
}


/* =================================================================== */
/* ========================= Bad hack jobs! ========================== */


typedef struct {
    SOCKET s;
    LPSOCKADDR name;
    int namelen;
    PVOID lpSendBuffer;
    LPOVERLAPPED lpOverlapped;

} ConnectJob;

DWORD WINAPI
ConnectThread (LPVOID lpParam)
{
    int code;
    ConnectJob *job = lpParam;
    BufferInfo *bufPtr;

    bufPtr = CONTAINING_RECORD(job->lpOverlapped, BufferInfo, ol);
    code = winSock.connect(job->s, job->name, job->namelen);
    if (code == SOCKET_ERROR) {
	bufPtr->WSAerr = winSock.WSAGetLastError();
    }
    PostQueuedCompletionStatus(IocpSubSystem.port, 0,
	    (ULONG_PTR) bufPtr->parent, job->lpOverlapped);
    IocpFree(job->name);
    IocpFree(job);
    return 0;
}

BOOL PASCAL
OurConnectEx (
    SOCKET s,
    const struct sockaddr* name,
    int namelen,
    PVOID lpSendBuffer,
    DWORD dwSendDataLength,
    LPDWORD lpdwBytesSent,
    LPOVERLAPPED lpOverlapped)
{
    ConnectJob *job;
    HANDLE thread;
    DWORD dummy;

    // 1) Create a thread and have the thread do the work.
    //    Return thread start status.
    // 2) thread will do a blocking connect() and possible send()
    //    should lpSendBuffer not be NULL and dwSendDataLength greater
    //    than zero.
    // 3) Notify the completion port with PostQueuedCompletionStatus().
    //    We don't exactly know the port associated to us, so assume the
    //    one we ALWAYS use (insider knowledge of ourselves).
    // 4) exit thread.

    job = IocpAlloc(sizeof(ConnectJob));
    job->s = s;
    job->name = IocpAlloc(namelen);
    memcpy(job->name, name, namelen);
    job->namelen = namelen;
    job->lpSendBuffer = lpSendBuffer;
    job->lpOverlapped = lpOverlapped;

    thread = CreateThread(NULL, 256, ConnectThread, job, 0, &dummy);

    if (thread) {
	/* remove local reference so the thread cleans up after it exits. */
	CloseHandle(thread);
	winSock.WSASetLastError(WSA_IO_PENDING);
    } else {
	winSock.WSASetLastError(GetLastError());
    }
    return FALSE;
}
