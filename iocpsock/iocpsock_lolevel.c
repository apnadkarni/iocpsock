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
Tcl_ThreadDataKey dataKey;

/* file scope globals. */
static DWORD numCPUs;


/* local prototypes */
static void			InitSockets();
static DWORD			InitializeIocpSubSystem();
static Tcl_ExitProc		IocpExitHandler;
static Tcl_ExitProc		IocpThreadExitHandler;
static Tcl_EventProc		IocpEventProc;

static Tcl_EventSetupProc	IocpEventSetupProc;
static Tcl_EventCheckProc	IocpEventCheckProc;

static Tcl_DriverCloseProc	IocpCloseProc;
static Tcl_DriverInputProc	IocpInputProc;
static Tcl_DriverOutputProc	IocpOutputProc;
static Tcl_DriverSetOptionProc	IocpSetOptionProc;
static Tcl_DriverGetOptionProc	IocpGetOptionProc;
static Tcl_DriverWatchProc	IocpWatchProc;
static Tcl_DriverGetHandleProc	IocpGetHandleProc;
static Tcl_DriverBlockModeProc	IocpBlockProc;

static void	    IocpAlertToTclNewAccept(SocketInfo *infoPtr,
			    SocketInfo *newClient, SOCKADDR_STORAGE *local,
			    int localLen, SOCKADDR_STORAGE *remote,
			    int remoteLen);
static void	    IocpAcceptOne (SocketInfo *infoPtr);
static void	    FreeBufferObj(BufferInfo *obj);
static void	    FreeSocketInfo(SocketInfo *infoPtr);
static DWORD	    PostOverlappedRecv(SocketInfo *infoPtr,
			    BufferInfo *recvobj);
static DWORD	    PostOverlappedSend(SocketInfo *infoPtr,
			    BufferInfo *sendobj);
//static void	    InsertPendingSend(SocketInfo *infoPtr,
//			    BufferInfo *send);
//static int	    DoSends(SocketInfo *infoPtr);
static void	    HandleIo(SocketInfo *infoPtr, BufferInfo *bufPtr,
			    HANDLE compPort, DWORD bytes, DWORD error,
			    DWORD flags);
static DWORD WINAPI CompletionThread(LPVOID lpParam);


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
	    } else {
		// TODO (long-term): put the old WSAAsyncSelect channel driver code in here too.
		/* winsockLoadErr = InitializeOldSubSystem(); */
		Tcl_Panic("Barf! Can't run IOCP on non-NT systems, sorry.");
	    }
	    if (winsockLoadErr != NO_ERROR) {
		goto unloadLibrary;
	    }
	}
    }

    if (tsdPtr->threadId == 0) {
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
    IocpSubSystem.heap = HeapCreate(0, 0, 0);
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



    // TODO: remove me when out-of-order issue is solved!
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
	SetThreadIdealProcessor(IocpSubSystem.threads[i], i);
    }

    Tcl_CreateEventSource(IocpEventSetupProc, IocpEventCheckProc, NULL);
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
//    IocpLLDestroy(tsdPtr->readySockets, 0);
}

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

    EnterCriticalSection(&tsdPtr->readySockets->lock);
    evCount = tsdPtr->readySockets->lCount;
    LeaveCriticalSection(&tsdPtr->readySockets->lock);

    while (evCount--) {
	infoPtr = IocpLLPopFront(tsdPtr->readySockets, IOCP_LL_NODESTROY);
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
IocpEventProc(
    Tcl_Event *evPtr,		/* Event to service. */
    int flags)			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    SocketInfo *infoPtr = ((SocketEvent *)evPtr)->infoPtr;
    int readyMask = 0;

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }

    /*
     * If an accept is ready, pop just one.  We can't pop all of them
     * or it would be greedy to the event loop and cause the granularity
     * to increase -- which is bad and holds back Tk from doing redraws.
     */

    if (infoPtr->readyAccepts != NULL
	    && IocpLLIsNotEmpty(infoPtr->readyAccepts)) {
	IocpAcceptOne(infoPtr);
	return 1;
    }

    if (IocpLLIsNotEmpty(infoPtr->llPendingRecv)) {
	readyMask |= TCL_READABLE;
    }
    if (IocpLLIsNotEmpty(infoPtr->llPendingSend)) {
	readyMask |= TCL_WRITABLE;
    }

    Tcl_NotifyChannel(infoPtr->channel, readyMask);
    return 1;
}

void
IocpAcceptOne (SocketInfo *infoPtr)
{
    char channelName[16 + TCL_INTEGER_SPACE];
    IocpAcceptInfo *acptInfo;

    acptInfo = IocpLLPopFront(infoPtr->readyAccepts, 0);
    wsprintfA(channelName, "iocp%d", acptInfo->clientInfo->socket);
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
CreateSocketAddress(
     const char *addr,
     const char *port,
     WS2ProtocolData *pdata,
     LPADDRINFO *paddrinfo)
{
    ADDRINFO hints;
    LPADDRINFO phints;
    int result;

    if (pdata != NULL) {
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_flags  = ((addr) ? 0 : AI_PASSIVE);
	hints.ai_family = pdata->af;
	hints.ai_socktype = pdata->type;
	hints.ai_protocol = pdata->protocol;
	phints = &hints;
    } else {
	phints = NULL;
    }

    result = getaddrinfo(addr, port, phints, paddrinfo);

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
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    int errorCode = 0;

    // TODO: use DisconnectEx() ???

    // The core wants to close channels after the exit handler!
    // Our heap is gone!
    if (initialized) {
	if (winSock.closesocket(infoPtr->socket) == SOCKET_ERROR) {
	    //TclWinConvertWSAError((DWORD) winSock.WSAGetLastError());
	    errorCode = Tcl_GetErrno();
	}
//	FreeSocketInfo(infoPtr);
    }

    return errorCode;
}

static int
IocpInputProc(instanceData, buf, toRead, errorCodePtr)
    ClientData instanceData;	/* The socket state. */
    char *buf;			/* Where to store data. */
    int toRead;			/* Maximum number of bytes to read. */
    int *errorCodePtr;		/* Where to store error codes. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    char *bufPos = buf;
    int bytesRead = 0;
    BufferInfo *bufPtr;

    /*
     * Merge as much as toRead will allow.
     */

    while ((bufPtr = IocpLLPopFront(infoPtr->llPendingRecv, 0)) != NULL) {
	if (bytesRead + (int) bufPtr->used > toRead) {
	    /* Too large.  Push it back on for later. */
	    IocpLLPushFront(infoPtr->llPendingRecv, bufPtr, &bufPtr->node);
	    IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr, NULL);
	    break;
	}
	memcpy(bufPos, bufPtr->buf, bufPtr->used);
	bytesRead += bufPtr->used;
	bufPos += bufPtr->used;
	FreeBufferObj(bufPtr);
    }

    return bytesRead;
}

static int
IocpOutputProc(instanceData, buf, toWrite, errorCodePtr)
    ClientData instanceData;	/* The socket state. */
    CONST char *buf;		/* Where to get data. */
    int toWrite;		/* Maximum number of bytes to write. */
    int *errorCodePtr;		/* Where to store error codes. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    BufferInfo *bufPtr;
    DWORD code;

    bufPtr = GetBufferObj(infoPtr, toWrite);
    memcpy(bufPtr->buf, buf, toWrite);
    code = PostOverlappedSend(infoPtr, bufPtr);

    if (code != NO_ERROR) {
	// TODO: what SHOULD go here?
    }

    return toWrite;
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

// TODO: pass this also to a protocol specific option routine.

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
	if (infoPtr->lastError != NO_ERROR) {
	    Tcl_DStringAppend(dsPtr, GetSysMsg(infoPtr->lastError), -1);
	    infoPtr->lastError = NO_ERROR;
	}
	return TCL_OK;
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 'p') &&
                    (strncmp(optionName, "-peername", len) == 0))) {
        if (infoPtr->remoteAddr == NULL) {
	    infoPtr->remoteAddr = (LPSOCKADDR) IocpAlloc(infoPtr->proto->addrLen);
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
	    infoPtr->localAddr = (LPSOCKADDR) IocpAlloc(infoPtr->proto->addrLen);
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
    ClientData instanceData;	/* The socket state. */
    int mode;			/* TCL_MODE_BLOCKING or
                                 * TCL_MODE_NONBLOCKING. */
{
    // TODO: add blocking emulation to IOCP (makes me sick).
    return 0;
}

static int
IocpGetHandleProc(instanceData, direction, handlePtr)
    ClientData instanceData;	/* The socket state. */
    int direction;		/* TCL_READABLE or TCL_WRITABLE */
    ClientData *handlePtr;	/* Where to store the handle.  */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;

    *handlePtr = (ClientData) infoPtr->socket;
    return TCL_OK;
}


/* takes buffer ownership */
void
IocpAlertToTclNewAccept (
    SocketInfo *infoPtr,
    SocketInfo *newClient,
    SOCKADDR_STORAGE *LocalSockaddr,
    int LocalSockaddrLen,
    SOCKADDR_STORAGE *RemoteSockaddr,
    int RemoteSockaddrLen)
{
    IocpAcceptInfo *acptInfo;

    acptInfo = (IocpAcceptInfo *) IocpAlloc(sizeof(IocpAcceptInfo));
    if (acptInfo == NULL) {
	OutputDebugString("IocpAlertToTclNewAccept: HeapAlloc failed: ");
	OutputDebugString(GetSysMsg(GetLastError()));
	OutputDebugString("\n");
	return;
    }

    memcpy(&acptInfo->local, LocalSockaddr, LocalSockaddrLen);
    acptInfo->localLen = LocalSockaddrLen;
    memcpy(&acptInfo->remote, RemoteSockaddr, RemoteSockaddrLen);
    acptInfo->remoteLen = RemoteSockaddrLen;
    acptInfo->clientInfo = newClient;

    /*
     * Queue this accept's data into the listening channel's info block.
     */
    IocpLLPushBack(infoPtr->readyAccepts, acptInfo, NULL);

    /*
     * Let IocpCheckProc() know this channel has something ready
     * that needs servicing.
     */
    IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr, NULL);

    /* In case Tcl is asleep in the notifier, wake it up. */
    Tcl_ThreadAlert(infoPtr->tsdHome->threadId);
}


SocketInfo *
NewSocketInfo(socket)
    SOCKET socket;
{
    SocketInfo *infoPtr;

    infoPtr = (SocketInfo *) IocpAlloc(sizeof(SocketInfo));
    infoPtr->socket = socket;
    infoPtr->readyAccepts = NULL;  
    infoPtr->acceptProc = NULL;
    infoPtr->localAddr = NULL;	    /* Local sockaddr. */
    infoPtr->remoteAddr = NULL;	    /* Remote sockaddr. */
    infoPtr->lastError = NO_ERROR;
    infoPtr->bClosing = FALSE;
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
FreeSocketInfo(SocketInfo *infoPtr)
{
    DeleteCriticalSection(&infoPtr->critSec);

    if (infoPtr->localAddr) IocpFree(infoPtr->localAddr);
    if (infoPtr->remoteAddr) IocpFree(infoPtr->remoteAddr);

    if (infoPtr->readyAccepts) {
	IocpLLDestroy(infoPtr->readyAccepts, 0);
    }
    IocpFree(infoPtr);
}

BufferInfo *
GetBufferObj(SocketInfo *si, size_t buflen)
{
    BufferInfo *newobj=NULL;

    // Allocate the object
    newobj = (BufferInfo *) IocpAlloc(sizeof(BufferInfo));
    if (newobj == NULL) {
	OutputDebugString("GetBufferObj: HeapAlloc failed: ");
	OutputDebugString(GetSysMsg(GetLastError()));
	OutputDebugString("\n");
	return 0L;
    }
    // Allocate the buffer
    newobj->buf = (BYTE *) IocpAlloc(sizeof(BYTE)*buflen);
    if (newobj->buf == NULL) {
	OutputDebugString("GetBufferObj: HeapAlloc failed: ");
	OutputDebugString(GetSysMsg(GetLastError()));
	OutputDebugString("\n");
	return 0L;
    }
    newobj->buflen = buflen;
    newobj->addrlen = sizeof(newobj->addr);
    return newobj;
}

void
FreeBufferObj(BufferInfo *bufPtr)
{
    IocpFree(bufPtr->buf);
    IocpFree(bufPtr);
}

SocketInfo *
NewAcceptSockInfo(SOCKET s, SocketInfo *infoPtr)
{
    SocketInfo *newInfoPtr;

    newInfoPtr = (SocketInfo *) IocpAlloc(sizeof(SocketInfo));
    if (newInfoPtr == NULL)
    {
	return NULL;
    }

    // Initialize the members
    newInfoPtr->socket = s;
    newInfoPtr->proto = infoPtr->proto;
    newInfoPtr->tsdHome = infoPtr->tsdHome;

    // For TCP we initialize the IO count to one since the AcceptEx is posted
    //    to receive data
//    sockobj->IoCountIssued = ((si->proto->protocol == IPPROTO_TCP) ? 1 : 0);

    InitializeCriticalSection(&newInfoPtr->critSec);
    newInfoPtr->llPendingRecv = IocpLLCreate(); //Our pending recv list.
    newInfoPtr->llPendingSend = IocpLLCreate(); //Our pending send list.

    return newInfoPtr;
}


DWORD
PostOverlappedAccept(SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    DWORD bytes, err;
    int rc;
    unsigned int addr_stoage = sizeof(SOCKADDR_STORAGE) + 16;

    bufPtr->operation = OP_ACCEPT;

    /*
     * Create a ready client socket of the same type for a future
     * incoming connection.
     */

    bufPtr->socket = winSock.WSASocketA(infoPtr->proto->af,
	    infoPtr->proto->type, infoPtr->proto->protocol, NULL, 0,
	    WSA_FLAG_OVERLAPPED);

    if (bufPtr->socket == INVALID_SOCKET) {
	return SOCKET_ERROR;
    }

    /*
     * The buffer can not be smaller than this.
     */

    _ASSERT(bufPtr->buflen >= ((sizeof(SOCKADDR_STORAGE) + 16) * 2));

    /*
     * Use the special function for overlapped accept() that is provided
     * by the LSP of this socket type.
     */

    rc = infoPtr->proto->AcceptEx(infoPtr->socket, bufPtr->socket,
	    bufPtr->buf, bufPtr->buflen - (addr_stoage * 2),
	    addr_stoage, addr_stoage, &bytes, &bufPtr->ol);

    err = winSock.WSAGetLastError();
    if (rc == FALSE) {
	if (err != WSA_IO_PENDING) {
	    //GetSysMsg(err);
	    return SOCKET_ERROR;
	}
	/*
	 * Increment the outstanding overlapped count for this socket.
	 */
	InterlockedIncrement(&infoPtr->OutstandingOps);
    } else {
	/* the AcceptEx operation completed now, instead of being posted. */
        HandleIo(infoPtr, bufPtr, IocpSubSystem.port, bytes, err, 0);
    }

    return NO_ERROR;
}

DWORD
PostOverlappedRecv(SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    WSABUF wbuf;
    DWORD bytes, flags, err;
    int rc;

    bufPtr->operation = OP_READ;
    wbuf.buf = bufPtr->buf;
    wbuf.len = bufPtr->buflen;
    flags = 0;

    EnterCriticalSection(&infoPtr->critSec);

    // Assign the IO order to this receive. This must be performned within
    //    the critical section. The operation of assigning the IO count and posting
    //    the receive cannot be interupted.
    bufPtr->IoOrder = infoPtr->IoCountIssued;
    infoPtr->IoCountIssued++;

    if (infoPtr->proto->type == SOCK_STREAM) {
	rc = winSock.WSARecv(infoPtr->socket, &wbuf, 1, &bytes, &flags,
		&bufPtr->ol, NULL);
    } else {
	rc = winSock.WSARecvFrom(infoPtr->socket, &wbuf, 1, &bytes, &flags,
		(SOCKADDR *)&bufPtr->addr, &bufPtr->addrlen,
		&bufPtr->ol, NULL);
    }

    LeaveCriticalSection(&infoPtr->critSec);

    err = winSock.WSAGetLastError();
    if (rc == SOCKET_ERROR) {
	if (err != WSA_IO_PENDING) {
	    //GetSysMsg(err);
	    return SOCKET_ERROR;
	}
	/*
	 * Increment the outstanding overlapped count for this socket.
	 */
	InterlockedIncrement(&infoPtr->OutstandingOps);
    } else {
	/* the WSARecv(From) operation completed now, instead of being posted. */
        HandleIo(infoPtr, bufPtr, IocpSubSystem.port, bytes, err, flags);
    }

    return NO_ERROR;
}

DWORD
PostOverlappedSend(SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    WSABUF wbuf;
    DWORD bytes, err;
    int rc;

    bufPtr->operation = OP_WRITE;
    wbuf.buf = bufPtr->buf;
    wbuf.len = bufPtr->buflen;

    EnterCriticalSection(&infoPtr->critSec);

    /* Incrmenting the last send issued and issuing the send should not
     * be interuptable. */
    infoPtr->LastSendIssued++;

    if (infoPtr->proto->type == SOCK_STREAM) {
	rc = winSock.WSASend(infoPtr->socket, &wbuf, 1, &bytes, 0,
		&bufPtr->ol, NULL);
    } else {
	rc = winSock.WSASendTo(infoPtr->socket, &wbuf, 1, &bytes, 0,
                (SOCKADDR *)&bufPtr->addr, bufPtr->addrlen,
		&bufPtr->ol, NULL);
    }

    LeaveCriticalSection(&infoPtr->critSec);

    err = winSock.WSAGetLastError();
    if (rc == SOCKET_ERROR) {
	if (err != WSA_IO_PENDING) {
	    //GetSysMsg(err);
	    return SOCKET_ERROR;
	}
	/*
	 * Increment the outstanding overlapped count for this socket.
	 */
	InterlockedIncrement(&infoPtr->OutstandingOps);
    } else {
	/* the WSASend(To) operation completed now, instead of being posted. */
        HandleIo(infoPtr, bufPtr, IocpSubSystem.port, bytes, err, 0);
    }

    return NO_ERROR;
}

/*
void
InsertPendingSend(SocketInfo *infoPtr, BufferInfo *send)
{
    BufferInfo *ptr, *prev = NULL;


    EnterCriticalSection(&infoPtr->critSec);
    send->next = NULL;

    // This loop finds the place to put the send within the list.
    //    The send list is in the same order as the receives were
    //    posted.
    ptr = infoPtr->OutOfOrderSends;
    while (ptr) {
	if (send->IoOrder < ptr->IoOrder) {
	    break;
	}
	prev = ptr;
	ptr = ptr->next;
    }
    if (prev == NULL) {
	// Inserting at head
	infoPtr->OutOfOrderSends = send;
	send->next = ptr;
    } else {
	// Insertion somewhere in the middle
	prev->next = send;
	send->next = ptr;
    }

    LeaveCriticalSection(&infoPtr->critSec);
}

static int
DoSends(SocketInfo *infoPtr)
{
    BufferInfo *bufPtr;
    int ret = NO_ERROR;

    ret = NO_ERROR;
    EnterCriticalSection(&infoPtr->critSec);

    bufPtr = infoPtr->OutOfOrderSends;
    while ((bufPtr) && (bufPtr->IoOrder == infoPtr->LastSendIssued)) {
        if (PostOverlappedSend(infoPtr, bufPtr) != NO_ERROR) {
            FreeBufferObj(bufPtr);
            ret = SOCKET_ERROR;
            break;
        }
        infoPtr->OutOfOrderSends = bufPtr = bufPtr->next;
    }

    LeaveCriticalSection(&infoPtr->critSec);
    return ret;
}
*/

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
    SocketInfo *infoPtr;
    BufferInfo *bufPtr;
    OVERLAPPED *lpOverlapped = NULL;
    DWORD BytesTransfered, Flags, Error;
    BOOL  ok;

    for (;;) {
	Error = NO_ERROR;

	ok = GetQueuedCompletionStatus(cpinfo->port,
		&BytesTransfered, (PULONG_PTR)&infoPtr,
		&lpOverlapped, INFINITE);

	if (ok && !infoPtr) {
	    /* A NULL key indicates closure time for this thread. */
	    break;
	}

	/*
	 * Use the pointer to the overlapped structure and derive from it
	 * the top of the parent BufferInfo structure it sits in.  If the
	 * position of the overlapped structure moves around within the
	 * BufferInfo structure declaration, this function will _not_ need
	 * to be modified.
	 */

	bufPtr = CONTAINING_RECORD(lpOverlapped, BufferInfo, ol);

	if (!ok) {
	    /*
	     * If GetQueuedCompletionStatus() returned a failure on the
	     * operation, call WSAGetOverlappedResult() to translate the
	     * error into a Winsock error code.
	     */

	    ok = winSock.WSAGetOverlappedResult(infoPtr->socket,
		    lpOverlapped, &BytesTransfered, FALSE, &Flags);

	    if (!ok) {
		Error = winSock.WSAGetLastError();
	    }
	}

	/* Go handle the IO operation. */
        HandleIo(infoPtr, bufPtr, cpinfo->port, BytesTransfered, Error,
		Flags);
    }
    return 0;
}

void
HandleIo (
    SocketInfo *infoPtr,
    BufferInfo *bufPtr,
    HANDLE CompPort,
    DWORD bytes,
    DWORD error,
    DWORD flags)
{
    SocketInfo *newInfoPtr;     // New client object for accepted connections
    BufferInfo *newBufPtr;       // Used to post new receives on accepted connections
    BOOL bCleanupSocket = FALSE;

    if (error == ERROR_OPERATION_ABORTED) {
        FreeBufferObj(bufPtr);
        if (InterlockedDecrement(&infoPtr->OutstandingOps) == 0) {
	    FreeSocketInfo(infoPtr);
        }
	return;
    }

    bufPtr->used = bytes;
    bufPtr->error = error;

    EnterCriticalSection(&infoPtr->critSec);
    if (bufPtr->operation == OP_ACCEPT) {
        HANDLE hcp;
        SOCKADDR_STORAGE *local = NULL, *remote = NULL;
        int localLen, remoteLen;

        /*
	 * Get the address information that is specific to this LSP's
	 * decoder.
	 */
        infoPtr->proto->GetAcceptExSockaddrs(bufPtr->buf,
		bufPtr->buflen - ((sizeof(SOCKADDR_STORAGE) + 16) * 2),
                sizeof(SOCKADDR_STORAGE) + 16,
		sizeof(SOCKADDR_STORAGE) + 16,
		(SOCKADDR **)&local, &localLen,
                (SOCKADDR **)&remote, &remoteLen);

	// TODO: Is this correct?
	winSock.setsockopt(bufPtr->socket, SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT, (char *)&infoPtr->socket,
		sizeof(SOCKET));

        /* Get a new SocketInfo for the new client connection. */
        newInfoPtr = NewAcceptSockInfo(bufPtr->socket, infoPtr);
	newInfoPtr->localAddr = (LPSOCKADDR) IocpAlloc(newInfoPtr->proto->addrLen);
	memcpy(newInfoPtr->localAddr, local, localLen);
	newInfoPtr->remoteAddr = (LPSOCKADDR) IocpAlloc(newInfoPtr->proto->addrLen);
	memcpy(newInfoPtr->remoteAddr, remote, remoteLen);

	/* Alert Tcl to this new connection. */
	IocpAlertToTclNewAccept(infoPtr, newInfoPtr, local, localLen,
		remote, remoteLen);

	/* (takes buffer ownership) */
	IocpLLPushBack(newInfoPtr->llPendingRecv, bufPtr, &bufPtr->node);

	/*
	 * Let IocpCheckProc() know this new channel has a ready read
	 * that needs servicing.
	 */
	IocpLLPushBack(newInfoPtr->tsdHome->readySockets, newInfoPtr, NULL);
	/* Should the notifier be asleep, zap it awake. */
	Tcl_ThreadAlert(newInfoPtr->tsdHome->threadId);

        /* Associate the new socket to our completion port. */
        hcp = CreateIoCompletionPort((HANDLE)newInfoPtr->socket, CompPort,
                (ULONG_PTR)newInfoPtr, 0);

        if (hcp == NULL) {
//            fprintf(stderr, "CompletionThread: CreateIoCompletionPort failed: %d\n",
//                    GetLastError());
            return;
        }

        /* Now post a receive on this new connection. */
        newBufPtr = GetBufferObj(newInfoPtr, 256);
        if (PostOverlappedRecv(newInfoPtr, newBufPtr) != NO_ERROR) {
            /* If for some reason the send call fails, clean up the connection. */
            FreeBufferObj(newBufPtr);
            error = SOCKET_ERROR;
        }
        
        /*
	 * Post another new AcceptEx() to replace this one that just fired.
	 *
	 * This call could cause recurrsion..
	 * Careful!  If PostOverlappedAccept() gets serviced by winsock
	 * immediatly, rather than posted, we could end-up back here again.
	 * That some heavy load :)
	 */

        newBufPtr = GetBufferObj(infoPtr, 512);
        PostOverlappedAccept(infoPtr, newBufPtr);

    } else if (bufPtr->operation == OP_READ) {

	/* (takes buffer ownership) */
	IocpLLPushBack(infoPtr->llPendingRecv, bufPtr, &bufPtr->node);

	/*
	 * Let IocpCheckProc() know this new channel has a ready read
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

	    newBufPtr = GetBufferObj(infoPtr, 256);
	    if (PostOverlappedRecv(infoPtr, newBufPtr) != NO_ERROR) {
		/* In the event the recv fails, clean up the connection */
		FreeBufferObj(newBufPtr);
		error = SOCKET_ERROR;
	    }
        }
    } else if (bufPtr->operation == OP_WRITE) {

        FreeBufferObj(bufPtr);

//        if (DoSends(infoPtr) != NO_ERROR) {
//            dbgprint("Cleaning up inside OP_WRITE handler\n");
//            error = SOCKET_ERROR;
//        }
    }

    LeaveCriticalSection(&infoPtr->critSec);

    return;
}

__inline LPVOID
IocpAlloc (SIZE_T size)
{
    return HeapAlloc(IocpSubSystem.heap, HEAP_ZERO_MEMORY, size);
}

__inline BOOL
IocpFree (LPVOID block)
{
    return HeapFree(IocpSubSystem.heap, 0, block);
}

/* Bitmask macros. */
#define mask_a( mask, val ) if ( ( mask & val ) != val ) { mask |= val; }
#define mask_d( mask, val ) if ( ( mask & val ) == val ) { mask ^= val; }
#define mask_y( mask, val ) ( mask & val ) == val
#define mask_n( mask, val ) ( mask & val ) != val


/* Creates a linked list. */
LPLLIST
IocpLLCreate ()
{   
    LPLLIST ll;
    
    /* Alloc a linked list. */
    ll = (LPLLIST) IocpAlloc(sizeof(LLIST));
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

//Destroyes a linked list.
BOOL 
IocpLLDestroy(
    LPLLIST ll,
    DWORD dwState)
{   
    //Destroy the linked list.
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (ll->lCount) {
//	cout << "\nLinked List Memory Leak: " << ll->lCount << " items\n";
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    DeleteCriticalSection(&ll->lock);
    return HeapFree(IocpSubSystem.heap, 0, ll);
}

//Adds an item to the end of the list.
LPLLNODE 
IocpLLPushBack(
    LPLLIST ll,
    LPVOID lpItem,
    LPLLNODE pnode)
{
    LPLLNODE tmp;

    EnterCriticalSection(&ll->lock);
    if (!pnode) {
	pnode = (LPLLNODE) IocpAlloc(sizeof(LLNODE));
    }
    if (!pnode) {
	LeaveCriticalSection(&ll->lock);
	return NULL;
    }
    pnode->lpItem = lpItem;
    if (!ll->front && ! ll->back ) {
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

//Adds an item to the front of the list.
LPLLNODE 
IocpLLPushFront(
    LPLLIST ll,
    LPVOID lpItem,
    LPLLNODE pnode)
{
    BOOL alloc;
    LPLLNODE tmp;

    EnterCriticalSection(&ll->lock);
    alloc = FALSE;
    if (!pnode) {
	pnode = (LPLLNODE) IocpAlloc(sizeof(LLNODE));
	alloc = TRUE;
    }
    if (!pnode) {
	LeaveCriticalSection(&ll->lock);
	return NULL;
    }
    pnode->lpItem = lpItem;
    if (!ll->front && ! ll->back) {
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

//Removes all items from the list.
BOOL 
IocpLLPopAll(
    LPLLIST ll,
    LPLLNODE snode,
    DWORD dwState)
{
    LPLLNODE tmp1, tmp2;

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
	//Delete the node and decrement the counter.
        if (mask_n(dwState, IOCP_LL_NODESTROY)) {
	    tmp1->ll = NULL;
	    tmp1->next = NULL; 
            tmp1->prev = NULL;
	} else {
	    IocpLLNodeDestroy(tmp1);
	}
        ll->lCount--;
	tmp1 = tmp2;
    }

    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    
    return TRUE;
}

//Removes an item from the list.
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

    /* Delete the node and decrement the counter. */
    if (mask_n(dwState, IOCP_LL_NODESTROY)) {
	node->ll = NULL;
	node->next = NULL; 
        node->prev = NULL;
    } else {
	IocpLLNodeDestroy(node);
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

//Destroys a node.
BOOL
IocpLLNodeDestroy(
    LPLLNODE node)
{
    return IocpFree(node);
}

//Removes the item at the back of the list.
LPVOID
IocpLLPopBack(
    LPLLIST ll,
    DWORD dwState)
{
    LPLLNODE tmp;
    LPVOID vp;

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

//Removes the item at the front of the list.
LPVOID
IocpLLPopFront(
    LPLLIST ll,
    DWORD dwState)
{
    LPLLNODE tmp;
    LPVOID vp;

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

BOOL
IocpLLIsNotEmpty(LPLLIST ll)
{
    return (ll->lCount != 0 ? TRUE : FALSE);
}