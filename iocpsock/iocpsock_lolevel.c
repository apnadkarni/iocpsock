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

/* file-scope globals */
GUID AcceptExGuid		= WSAID_ACCEPTEX;
GUID GetAcceptExSockaddrsGuid	= WSAID_GETACCEPTEXSOCKADDRS;
GUID ConnectExGuid		= WSAID_CONNECTEX;
GUID DisconnectExGuid		= WSAID_DISCONNECTEX;
GUID TransmitFileGuid		= WSAID_TRANSMITFILE;
GUID TransmitPacketsGuid	= WSAID_TRANSMITPACKETS;
GUID WSARecvMsgGuid		= WSAID_WSARECVMSG;
Tcl_ThreadDataKey dataKey;

/* local prototypes */
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

#ifdef TCL_CHANNEL_VERSION_4
static Tcl_DriverCutProc	IocpCutProc;
static Tcl_DriverSpliceProc	IocpSpliceProc;
#else
static void			IocpCutProc (ClientData instanceData);
static void			IocpSpliceProc (ClientData instanceData);
#endif

static Tcl_ChannelTypeVersion   IocpGetTclMaxChannelVer (
				    Tcl_ChannelTypeVersion maxAllowed);
static void			IocpZapTclNotifier (SocketInfo *infoPtr);
static void			IocpAlertToTclNewAccept (SocketInfo *infoPtr,
				    SocketInfo *newClient);
static void			IocpAcceptOne (SocketInfo *infoPtr);
static void			IocpPushRecvAlertToTcl(SocketInfo *infoPtr,
				    BufferInfo *bufPtr);
static DWORD			PostOverlappedSend (SocketInfo *infoPtr,
				    BufferInfo *bufPtr);
static DWORD			PostOverlappedDisconnect (SocketInfo *infoPtr,
				    BufferInfo *bufPtr);
static DWORD WINAPI		CompletionThreadProc (LPVOID lpParam);
static void			HandleIo(register SocketInfo *infoPtr,
				    register BufferInfo *bufPtr,
				    HANDLE compPort, DWORD bytes, DWORD error,
				    DWORD flags);

/* special hack jobs! */
static BOOL PASCAL	OurConnectEx(SOCKET s,
			    const struct sockaddr* name, int namelen,
			    PVOID lpSendBuffer, DWORD dwSendDataLength,
			    LPDWORD lpdwBytesSent,
			    LPOVERLAPPED lpOverlapped);
static BOOL PASCAL	OurDisonnectEx(SOCKET hSocket,
			    LPOVERLAPPED lpOverlapped, DWORD dwFlags,
			    DWORD reserved);

/*
 * This structure describes the channel type structure for TCP socket
 * based I/O using the native overlapped interface along with completion
 * ports for maximum efficiency so that most operation are done entirely
 * in kernel-mode.
 */

Tcl_ChannelType IocpChannelType = {
    "iocp",		    /* Type name. */
#ifdef TCL_CHANNEL_VERSION_4
    TCL_CHANNEL_VERSION_4,  /* v4 channel */
#else
    TCL_CHANNEL_VERSION_2,  /* v2 channel */
#endif
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
#ifdef TCL_CHANNEL_VERSION_4
    IocpCutProc,	    /* cut proc. */
    IocpSpliceProc,	    /* splice proc. */
#endif
};


typedef struct SocketEvent {
    Tcl_Event header;		/* Information that is standard for
				 * all events. */
    SocketInfo *infoPtr;
} SocketEvent;




/* =================================================================== */
/* ============= Initailization and shutdown procedures ============== */



ThreadSpecificData *
InitSockets()
{
    WSADATA wsaData;
    OSVERSIONINFO os;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    /* global/once init */
    if (!initialized) {
	initialized = 1;

#ifdef STATIC_BUILD
	iocpModule = TclWinGetTclInstance();
#endif

	ZeroMemory(&winSock, sizeof(WinsockProcs));

	/*
	 * Try ws2_32.dll first, if available.
	 */

	if (winSock.hModule = LoadLibraryA("ws2_32.dll"));
	else if (winSock.hModule = LoadLibraryA("wsock32.dll"));
	else if (winSock.hModule = LoadLibraryA("winsock.dll"));
	else {
	    winsockLoadErr = TCL_WSE_NOTFOUND;
	    return NULL;
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
	    winSock.WSAAddressToString = (LPFN_WSAADDRESSTOSTRINGA)
		    GetProcAddress(winSock.hModule, "WSAAddressToStringA");
	    winSock.WSACloseEvent = (LPFN_WSACLOSEEVENT)
		    GetProcAddress(winSock.hModule, "WSACloseEvent");
	    winSock.WSAConnect = (LPFN_WSACONNECT)
		    GetProcAddress(winSock.hModule, "WSAConnect");
	    winSock.WSACreateEvent = (LPFN_WSACREATEEVENT)
		    GetProcAddress(winSock.hModule, "WSACreateEvent");
	    winSock.WSADuplicateSocket = (LPFN_WSADUPLICATESOCKETA)
		    GetProcAddress(winSock.hModule, "WSADuplicateSocketA");
	    winSock.WSAEnumNameSpaceProviders = (LPFN_WSAENUMNAMESPACEPROVIDERSA)
		    GetProcAddress(winSock.hModule, "WSAEnumNameSpaceProvidersA");
	    winSock.WSAEnumNetworkEvents = (LPFN_WSAENUMNETWORKEVENTS)
		    GetProcAddress(winSock.hModule, "WSAEnumNetworkEvents");
	    winSock.WSAEnumProtocols = (LPFN_WSAENUMPROTOCOLSA)
		    GetProcAddress(winSock.hModule, "WSAEnumProtocolsA");
	    winSock.WSAEventSelect = (LPFN_WSAEVENTSELECT)
		    GetProcAddress(winSock.hModule, "WSAEventSelect");
	    winSock.WSAGetOverlappedResult = (LPFN_WSAGETOVERLAPPEDRESULT)
		    GetProcAddress(winSock.hModule, "WSAGetOverlappedResult");
	    winSock.WSAGetQOSByName = (LPFN_WSAGETQOSBYNAME)
		    GetProcAddress(winSock.hModule, "WSAGetQOSByName");
	    winSock.WSAGetServiceClassInfo = (LPFN_WSAGETSERVICECLASSINFO)
		    GetProcAddress(winSock.hModule, "WSAGetServiceClassInfoA");
	    winSock.WSAGetServiceClassNameByClassId = (LPFN_WSAGETSERVICECLASSNAMEBYCLASSIDA)
		    GetProcAddress(winSock.hModule, "WSAGetServiceClassNameByClassIdA");
	    winSock.WSAHtonl = (LPFN_WSAHTONL)
		    GetProcAddress(winSock.hModule, "WSAHtonl");
	    winSock.WSAHtons = (LPFN_WSAHTONS)
		    GetProcAddress(winSock.hModule, "WSAHtons");
	    winSock.WSAInstallServiceClass = (LPFN_WSAINSTALLSERVICECLASSA)
		    GetProcAddress(winSock.hModule, "WSAInstallServiceClassA");
	    winSock.WSAIoctl = (LPFN_WSAIOCTL)
		    GetProcAddress(winSock.hModule, "WSAIoctl");
	    winSock.WSAJoinLeaf = (LPFN_WSAJOINLEAF)
		    GetProcAddress(winSock.hModule, "WSAJoinLeaf");
	    winSock.WSALookupServiceBegin = (LPFN_WSALOOKUPSERVICEBEGINA)
		    GetProcAddress(winSock.hModule, "WSALookupServiceBeginA");
	    winSock.WSALookupServiceEnd = (LPFN_WSALOOKUPSERVICEEND)
		    GetProcAddress(winSock.hModule, "WSALookupServiceEnd");
	    winSock.WSALookupServiceNext = (LPFN_WSALOOKUPSERVICENEXTA)
		    GetProcAddress(winSock.hModule, "WSALookupServiceNextA");
	    winSock.WSANSPIoctl = (LPFN_WSANSPIOCTL)
		    GetProcAddress(winSock.hModule, "WSANSPIoctl");
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
	    winSock.WSARemoveServiceClass = (LPFN_WSAREMOVESERVICECLASS)
		    GetProcAddress(winSock.hModule, "WSARemoveServiceClass");
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
	    winSock.WSASetService = (LPFN_WSASETSERVICEA)
		    GetProcAddress(winSock.hModule, "WSASetServiceA");
	    winSock.WSASocket = (LPFN_WSASOCKETA)
		    GetProcAddress(winSock.hModule, "WSASocketA");
	    winSock.WSAStringToAddress = (LPFN_WSASTRINGTOADDRESSA)
		    GetProcAddress(winSock.hModule, "WSAStringToAddressA");
	    winSock.WSAWaitForMultipleEvents= (LPFN_WSAWAITFORMULTIPLEEVENTS)
		    GetProcAddress(winSock.hModule, "WSAWaitForMultipleEvents");

	    if ((winSock.WSAAccept == NULL) ||
		    (winSock.WSAAddressToString == NULL) ||
		    (winSock.WSACloseEvent == NULL) ||
		    (winSock.WSAConnect == NULL) ||
		    (winSock.WSACreateEvent == NULL) ||
		    (winSock.WSADuplicateSocket == NULL) ||
		    (winSock.WSAEnumNameSpaceProviders == NULL) ||
		    (winSock.WSAEnumNetworkEvents == NULL) ||
		    (winSock.WSAEnumProtocols == NULL) ||
		    (winSock.WSAEventSelect == NULL) ||
		    (winSock.WSAGetOverlappedResult == NULL) ||
		    (winSock.WSAGetQOSByName == NULL) ||
		    (winSock.WSAGetServiceClassInfo == NULL) ||
		    (winSock.WSAGetServiceClassNameByClassId == NULL) ||
		    (winSock.WSAHtonl == NULL) ||
		    (winSock.WSAHtons == NULL) ||
		    (winSock.WSAInstallServiceClass == NULL) ||
		    (winSock.WSAIoctl == NULL) ||
		    (winSock.WSAJoinLeaf == NULL) ||
		    (winSock.WSALookupServiceBegin == NULL) ||
		    (winSock.WSALookupServiceEnd == NULL) ||
		    (winSock.WSALookupServiceNext == NULL) ||
/* WinXP only, don't force a failure  (winSock.WSANSPIoctl == NULL) || */
		    (winSock.WSANtohl == NULL) ||
		    (winSock.WSANtohs == NULL) ||
		    (winSock.WSAProviderConfigChange == NULL) ||
		    (winSock.WSARecv == NULL) ||
		    (winSock.WSARecvDisconnect == NULL) ||
		    (winSock.WSARecvFrom == NULL) ||
		    (winSock.WSARemoveServiceClass == NULL) ||
		    (winSock.WSAResetEvent == NULL) ||
		    (winSock.WSASend == NULL) ||
		    (winSock.WSASendDisconnect == NULL) ||
		    (winSock.WSASendTo == NULL) ||
		    (winSock.WSASetEvent == NULL) ||
		    (winSock.WSASetService == NULL) ||
		    (winSock.WSASocket == NULL) ||
		    (winSock.WSAStringToAddress == NULL) ||
		    (winSock.WSAWaitForMultipleEvents == NULL)) {
		winsockLoadErr = TCL_WSE_NOTALL2XFUNCS;
		goto unloadLibrary;
	    }
	}

	/*
	 * Assert our Tcl_ChannelType struct to the true version this core
	 * can accept, but not above the version of our design.
	 */

	IocpChannelType.version =
		IocpGetTclMaxChannelVer(IocpChannelType.version);

	switch ((int)IocpChannelType.version) {
	    case TCL_CHANNEL_VERSION_1:
		/* Oldest Tcl_ChannelType struct. */
		IocpChannelType.version =
			(Tcl_ChannelTypeVersion) IocpBlockProc;
		break;
	    default:
		/* No other Tcl_ChannelType struct maniputions are known at
		 * this time. */
		break;
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

    return tsdPtr;

unloadLibrary:
    initialized = 0;
    FreeLibrary(winSock.hModule);
    winSock.hModule = NULL;
    return NULL;
}

Tcl_ChannelTypeVersion
IocpGetTclMaxChannelVer (Tcl_ChannelTypeVersion maxAllowed)
{
    Tcl_ChannelType fake;

    if (maxAllowed == (Tcl_ChannelTypeVersion) 0x1) {
	return maxAllowed;
    }

    /*
     * Stubs slot empty.  Must be TCL_CHANNEL_VERSION_1
     */
    if (Tcl_ChannelVersion == NULL) {
	return TCL_CHANNEL_VERSION_1;
    }
    
    /* Tcl_ChannelVersion only touches the ->version field. */
    fake.version = maxAllowed;

    while (Tcl_ChannelVersion(&fake) == TCL_CHANNEL_VERSION_1) {
	fake.version = (Tcl_ChannelTypeVersion)((int)fake.version - 1);
	if (fake.version == (Tcl_ChannelTypeVersion) 0x1) break;
    }
    return fake.version;
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
    DWORD error = NO_ERROR;
    SYSTEM_INFO si;

    GetSystemInfo(&si);

    /* Create the completion port. */
    IocpSubSystem.port = CreateIoCompletionPort(
	    INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)NULL, 0);
    if (IocpSubSystem.port == NULL) {
	error = GetLastError();
	goto done;
    }

    /* Create the general private memory heap. */
    IocpSubSystem.heap = HeapCreate(0, IOCP_HEAP_START_SIZE, 0);
    if (IocpSubSystem.heap == NULL) {
	error = GetLastError();
	CloseHandle(IocpSubSystem.port);
	goto done;
    }

    /* Create the special private memory heap. */
    IocpSubSystem.NPPheap = HeapCreate(0, IOCP_HEAP_START_SIZE, 0);
    if (IocpSubSystem.NPPheap == NULL) {
	error = GetLastError();
	HeapDestroy(IocpSubSystem.heap);
	CloseHandle(IocpSubSystem.port);
	goto done;
    }

    /* Create the thread to service the completion port. */
    IocpSubSystem.thread = CreateThread(NULL, 0, CompletionThreadProc,
	    &IocpSubSystem, 0, NULL);
    if (IocpSubSystem.thread == NULL) {
	error = TCL_WSE_CANTSTARTHANDLERTHREAD;
	HeapDestroy(IocpSubSystem.heap);
	HeapDestroy(IocpSubSystem.NPPheap);
	CloseHandle(IocpSubSystem.port);
	goto done;
    }

    Tcl_CreateExitHandler(IocpExitHandler, NULL);

done:
    return error;
#undef IOCP_HEAP_START_SIZE
}

void
IocpExitHandler (ClientData clientData)
{
    if (initialized) {

	Tcl_DeleteEvents(IocpRemoveAllPendingEvents, NULL);

	/* Cause the waiting I/O handler threads to exit. */
	PostQueuedCompletionStatus(IocpSubSystem.port, 0, 0, 0);

	/* Wait for one to exit from the group. */
	WaitForSingleObject(IocpSubSystem.thread, INFINITE);
	CloseHandle(IocpSubSystem.thread);

	/* Close the completion port object. */
	CloseHandle(IocpSubSystem.port);

	/* Tear down the private memory heaps. */
	HeapDestroy(IocpSubSystem.heap);
	HeapDestroy(IocpSubSystem.NPPheap);

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
    ThreadSpecificData *tsdPtr = InitSockets();
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
    ThreadSpecificData *tsdPtr = InitSockets();
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
	EnterCriticalSection(&tsdPtr->readySockets->lock);
	infoPtr = IocpLLPopFront(tsdPtr->readySockets, IOCP_LL_NOLOCK, 0);
	/*
	 * Flop the ready toggle.  This is used to improve event loop
	 * efficiency to avoid unneccesary events being queued into the 
	 * readySockets list.
	 */
	InterlockedExchange(&infoPtr->ready, 0);
	LeaveCriticalSection(&tsdPtr->readySockets->lock);

	/*
	 * Not ready. accept in the Tcl layer hasn't happened yet or
	 * socket is in the middle of doing an async close.
	 */
	if (infoPtr->channel == NULL) {
	    continue;
	}
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

    /*
     * If an accept is ready, pop one only.
     */

    if (infoPtr->readyAccepts != NULL) {
	IocpAcceptOne(infoPtr);
	return 1;
    }

    /*
     * If there is at least one entry on the infoPtr->llPendingRecv list,
     * and the watch mask is set to notify for readable, the channel is
     * readable.
     */
    
    if (infoPtr->watchMask & TCL_READABLE &&
	    IocpLLIsNotEmpty(infoPtr->llPendingRecv)) {
	readyMask |= TCL_READABLE;
    }

    /*
     * If the watch mask is set to notify for writable events, the
     * socket is connected, and outstanding sends are less than the
     * resource cap allowed for this socket, the channel is writable.
     */

    if (infoPtr->watchMask & TCL_WRITABLE && infoPtr->llPendingRecv /* connected */
	    && infoPtr->outstandingSends < infoPtr->outstandingSendCap) {
	readyMask |= TCL_WRITABLE;
    }

    if (readyMask) {
	Tcl_NotifyChannel(infoPtr->channel, readyMask);
    } else {
	/* this was a useless queue.  I want to know why!!!! */
	__asm nop;
    }
    return 1;
}

/*
 *-----------------------------------------------------------------------
 * IocpAcceptOne --
 *
 *  Accept one connection from the listening socket.  Repost to the
 *  readySockets list if more are available.  By doing it this way,
 *  incoming connections aren't greedy.
 *
 *-----------------------------------------------------------------------
 */
static void
IocpAcceptOne (SocketInfo *infoPtr)
{
    char channelName[16 + TCL_INTEGER_SPACE];
    IocpAcceptInfo *acptInfo;

    acptInfo = IocpLLPopFront(infoPtr->readyAccepts, IOCP_LL_NODESTROY, 0);

    if (acptInfo == NULL) {
	/* Don't barf if the counts don't match. */
	return;
    }

    wsprintf(channelName, "iocp%lu", acptInfo->clientInfo->socket);
    acptInfo->clientInfo->channel = Tcl_CreateChannel(&IocpChannelType, channelName,
	    (ClientData) acptInfo->clientInfo, (TCL_READABLE | TCL_WRITABLE));
    if (Tcl_SetChannelOption(NULL, acptInfo->clientInfo->channel, "-translation",
	    "auto crlf") == TCL_ERROR) {
	Tcl_Close((Tcl_Interp *) NULL, acptInfo->clientInfo->channel);
	goto error;
    }
    if (Tcl_SetChannelOption(NULL, acptInfo->clientInfo->channel, "-eofchar", "")
	    == TCL_ERROR) {
	Tcl_Close((Tcl_Interp *) NULL, acptInfo->clientInfo->channel);
	goto error;
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

error:
    /* TODO: return error info to the trace routine. */

    IocpFree(acptInfo);

    /* Requeue another for the next checkProc iteration if another readyAccepts exists. */
    EnterCriticalSection(&infoPtr->tsdHome->readySockets->lock);
    if (IocpLLIsNotEmpty(infoPtr->readyAccepts)) {
	/*
	 * Flop the ready toggle.  This is used to improve event loop
	 * efficiency to avoid unneccesary events being queued into the 
	 * readySockets list.
	 */
	if (!InterlockedExchange(&infoPtr->ready, 1)) {
	    /* No entry on the ready list.  Insert one. */
	    IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr,
		    NULL, IOCP_LL_NOLOCK);
	}
    }
    LeaveCriticalSection(&infoPtr->tsdHome->readySockets->lock);
    return;
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
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    int errorCode = 0, err;

    /*
     * The core wants to close channels after the exit handler!
     * Our heap is gone!
     */
    if (initialized) {

	/* Flip the bit so no new stuff can ever come in again. */
	InterlockedExchange(&infoPtr->ready, 1);

	/* artificially increment the count. */
	InterlockedIncrement(&infoPtr->outstandingOps);

	/* Setting this means all incoming operations will get trashed. */
	infoPtr->flags |= IOCP_CLOSING;

	/* Tcl now doesn't recognize us anymore, so don't let this dangle. */
	infoPtr->channel = NULL;

	if (!(infoPtr->flags & IOCP_ASYNC)) {

	    /* The blocking close.  We wait for all references to return. */

	    /* Only send a disconnect when the socket is a stream one and is not
	     * a listening socket */
	    if (infoPtr->proto->type == SOCK_STREAM && !infoPtr->acceptProc) {
		err = winSock.WSASendDisconnect(infoPtr->socket, NULL);
	    } else {
		err = winSock.closesocket(infoPtr->socket);
		infoPtr->socket = INVALID_SOCKET;
	    }
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

	    if (InterlockedDecrement(&infoPtr->outstandingOps) > 0) {
		WaitForSingleObject(infoPtr->allDone, INFINITE);
	    }

	    /*
	     * Remove all events queued in the event loop for this socket.
	     * Ie. backwalk the bucket brigade, by goly.
	     */

	    Tcl_DeleteEvents(IocpRemovePendingEvents, infoPtr);

	    FreeSocketInfo(infoPtr);

	} else {

	    /* The non-blocking close.  We don't return any errors to Tcl
	     * and allow the instance to auto-destory itself.  Listening sockets
	     * are NEVER non-blocking. */

	    BufferInfo *bufPtr;

	    /* Remove all pending ready socket notices that have yet to be queued
	     * into Tcl's event loop. */
	    IocpLLPopAllCompare(infoPtr->tsdHome->readySockets, infoPtr, 0);

	    /* Remove all events queued in the event loop for this socket. */
	    Tcl_DeleteEvents(IocpRemovePendingEvents, infoPtr);

	    /* Decrement the artificial count. */
    	    if (InterlockedDecrement(&infoPtr->outstandingOps) > 0) {
		if (infoPtr->proto->type == SOCK_STREAM) {
		    bufPtr = GetBufferObj(infoPtr, 0);
		    PostOverlappedDisconnect(infoPtr, bufPtr);
		} else {
		    winSock.WSASendDisconnect(infoPtr->socket, NULL);
		}
	    } else {
		/* All pending operations have ended. */
		FreeSocketInfo(infoPtr);
	    }

	}
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

    if (infoPtr->flags & IOCP_EOF) {
	*errorCodePtr = ENOTCONN;
	return -1;
    }

    /*
     * Check for a background error on the last operations.
     */

    if (infoPtr->lastError) {
	IocpWinConvertWSAError(infoPtr->lastError);
	infoPtr->flags |= IOCP_EOF;
	goto error;
    }

    /* If we are async, don't block on the queue. */
    timeout = (infoPtr->flags & IOCP_ASYNC ? 0 : INFINITE);

    /*
     * Merge in as much as toRead will allow.
     */

    if ((!(infoPtr->flags & IOCP_ASYNC)) || IocpLLIsNotEmpty(infoPtr->llPendingRecv)) {
	while ((bufPtr = IocpLLPopFront(infoPtr->llPendingRecv,
		IOCP_LL_NODESTROY, timeout)) != NULL) {
	    if (bytesRead + (int) bufPtr->used > toRead || (bufPtr->used == 0 && bytesRead)) {
		if (toRead < IOCP_RECV_BUFSIZE) {
		    DbgPrint("\n\nIOCPsock (non-recoverable): possible infinite loop in IocpInputProc!\n\n");
		}
		/* Too large or have EOF.  Push it back on for later. */
		IocpLLPushFront(infoPtr->llPendingRecv, bufPtr,
			&bufPtr->node, 0);
		/* instance is still readable, validate the instance is on the ready list. */
		IocpZapTclNotifier(infoPtr);
		break;
	    }
	    if (bufPtr->WSAerr != NO_ERROR) {
		infoPtr->lastError = bufPtr->WSAerr;
		IocpWinConvertWSAError(infoPtr->lastError);
		FreeBufferObj(bufPtr);
		goto error;
	    } else {
		if (bufPtr->used == 0) {
		    infoPtr->flags |= IOCP_EOF;
		    return 0;
		}
		memcpy(bufPos, bufPtr->buf, bufPtr->used);
		bytesRead += bufPtr->used;
		bufPos += bufPtr->used;
	    }
	    FreeBufferObj(bufPtr);
	    /* When blocking, only read one. */
	    if (!(infoPtr->flags & IOCP_ASYNC)) break;
	}
	if (infoPtr->outstandingRecvCap == 1 && !(infoPtr->flags & IOCP_CLOSING)) {
	    BufferInfo *newBufPtr = GetBufferObj(infoPtr, toRead);
	    if (PostOverlappedRecv(infoPtr, newBufPtr, 0) != NO_ERROR) {
		FreeBufferObj(newBufPtr);
	    }
	}
    } else {
	/* If there's nothing to get, return EWOULDBLOCK. */
	*errorCodePtr = EWOULDBLOCK;
	bytesRead = -1;
    }

    return bytesRead;

error:
    *errorCodePtr = Tcl_GetErrno();
    return -1;
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

    *errorCodePtr = 0;


    if (TclInExit() || infoPtr->flags & IOCP_EOF || infoPtr->flags & IOCP_CLOSING) {
	*errorCodePtr = ENOTCONN;
	return -1;
    }

    /*
     * Check for a background error on the last operations.
     */

    if (infoPtr->lastError) {
	IocpWinConvertWSAError(infoPtr->lastError);
	infoPtr->flags |= IOCP_EOF;
	goto error;
    }

    bufPtr = GetBufferObj(infoPtr, toWrite);
    memcpy(bufPtr->buf, buf, toWrite);
    PostOverlappedSend(infoPtr, bufPtr);

    /*
     * Let errors come back through the completion port or else we risk
     * a time problem where readable data is ignored before the error
     * actually happened.
     */

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
    int Integer, rtn;

    infoPtr = (SocketInfo *) instanceData;
    sock = infoPtr->socket;

    if (!stricmp(optionName, "-keepalive")) {
	if (Tcl_GetBoolean(interp, value, &Integer) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Integer) val = TRUE;
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
	if (Tcl_GetBoolean(interp, value, &Integer) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Integer) val = TRUE;
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

    } else if (strcmp(optionName, "-backlog") == 0) {
	int i;
	if (Tcl_GetInt(interp, value, &Integer) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Integer < IOCP_ACCEPT_CAP) {
	    if (interp) {
		char buf[TCL_INTEGER_SPACE];
		TclFormatInt(buf, IOCP_ACCEPT_CAP);
		Tcl_AppendResult(interp,
			"only a positive integer not less than ", buf,
			" is allowed", NULL);
	    }
	    return TCL_ERROR;
	}
	infoPtr->outstandingAcceptCap = Integer;
	/* Now post them in, if outstandingAcceptCap is now larger. */
        for (
	    i = infoPtr->outstandingAccepts;
	    i < infoPtr->outstandingAcceptCap;
	    i++
	) {
	    BufferInfo *bufPtr;
	    bufPtr = GetBufferObj(infoPtr, 0);
	    if (PostOverlappedAccept(infoPtr, bufPtr, 0) != NO_ERROR) {
		/* Oh no, the AcceptEx failed. */
		FreeBufferObj(bufPtr); break;
		/* TODO: add error reporting */
	    }
        }
	return TCL_OK;

    } else if (strcmp(optionName, "-sendcap") == 0) {
	if (Tcl_GetInt(interp, value, &Integer) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Integer < 1) {
	    if (interp) {
		Tcl_AppendResult(interp,
			"only a positive integer greater than zero is allowed",
			NULL);
	    }
	    return TCL_ERROR;
	}
	InterlockedExchange(&infoPtr->outstandingSendCap, Integer);
	return TCL_OK;

    } else if (strcmp(optionName, "-recvburst") == 0) {
	if (Tcl_GetInt(interp, value, &Integer) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Integer < 1) {
	    if (interp) {
		Tcl_AppendResult(interp,
			"only a positive integer greater than zero is allowed",
			NULL);
	    }
	    return TCL_ERROR;
	}
	InterlockedExchange(&infoPtr->outstandingRecvCap, Integer);
	return TCL_OK;
    }

// TODO: pass this also to a protocol specific option routine.

    return Tcl_BadChannelOption(interp, optionName, "keepalive nagle backlog sendcap recvburst");
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
		infoPtr->flags |= IOCP_EOF;
	    }
	    return TCL_OK;
#if 1   /* for debugging only */
	} else if (strncmp(optionName, "-ops", len) == 0) {
	    TclFormatInt(buf, infoPtr->outstandingOps);
	    Tcl_DStringAppendElement(dsPtr, buf);
	    return TCL_OK;
	} else if (strncmp(optionName, "-ready", len) == 0) {
	    EnterCriticalSection(&infoPtr->tsdHome->readySockets->lock);
	    TclFormatInt(buf, infoPtr->ready);
	    LeaveCriticalSection(&infoPtr->tsdHome->readySockets->lock);
	    Tcl_DStringAppendElement(dsPtr, buf);
	    return TCL_OK;
	} else if (strncmp(optionName, "-readable", len) == 0) {
	    if (infoPtr->llPendingRecv != NULL) {
		EnterCriticalSection(&infoPtr->llPendingRecv->lock);
		TclFormatInt(buf, infoPtr->llPendingRecv->lCount);
		LeaveCriticalSection(&infoPtr->llPendingRecv->lock);
		Tcl_DStringAppendElement(dsPtr, buf);
		return TCL_OK;
	    } else {
		if (interp) {
		    Tcl_AppendResult(interp, "A listening socket is not readable, ever.", NULL);
		}
		return TCL_ERROR;
	    }
	} else if (strncmp(optionName, "-readyaccepts", len) == 0) {
	    if (infoPtr->readyAccepts != NULL) {
		EnterCriticalSection(&infoPtr->readyAccepts->lock);
		TclFormatInt(buf, infoPtr->readyAccepts->lCount);
		LeaveCriticalSection(&infoPtr->readyAccepts->lock);
		Tcl_DStringAppendElement(dsPtr, buf);
		return TCL_OK;
	    } else {
		if (interp) {
		    Tcl_AppendResult(interp, "Not a listening socket.", NULL);
		}
		return TCL_ERROR;
	    }
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

    if (len == 0 || !strncmp(optionName, "-backlog", len)) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-backlog");
        }
	TclFormatInt(buf, infoPtr->outstandingAccepts);
	Tcl_DStringAppendElement(dsPtr, buf);
	if (len > 0) return TCL_OK;
    }

    if (len == 0 || !strncmp(optionName, "-sendcap", len)) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-sendcap");
            Tcl_DStringStartSublist(dsPtr);
        }
	TclFormatInt(buf, infoPtr->outstandingSendCap);
	Tcl_DStringAppendElement(dsPtr, buf);
	TclFormatInt(buf, infoPtr->outstandingSends);
	Tcl_DStringAppendElement(dsPtr, buf);
        if (len == 0) {
            Tcl_DStringEndSublist(dsPtr);
        } else {
            return TCL_OK;
        }
    }

    if (len == 0 || !strncmp(optionName, "-recvburst", len)) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-recvburst");
            Tcl_DStringStartSublist(dsPtr);
        }
	TclFormatInt(buf, infoPtr->outstandingRecvCap);
	Tcl_DStringAppendElement(dsPtr, buf);
	TclFormatInt(buf, infoPtr->outstandingRecvs);
	Tcl_DStringAppendElement(dsPtr, buf);
        if (len == 0) {
            Tcl_DStringEndSublist(dsPtr);
        } else {
            return TCL_OK;
        }
    }

    if (len > 0) {
        return Tcl_BadChannelOption(interp, optionName,
		"peername sockname keepalive nagle backlog sendcap recvburst");
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

    if (!infoPtr->acceptProc) {
        infoPtr->watchMask = mask;
	if (!mask) {
	    return;
	}
	if (mask & TCL_READABLE && IocpLLIsNotEmpty(infoPtr->llPendingRecv)) {
	    /* instance is readable, validate the instance is on the ready list. */
	    IocpZapTclNotifier(infoPtr);
	} else if (mask & TCL_WRITABLE && infoPtr->llPendingRecv /* connected */
		&& infoPtr->outstandingSends < infoPtr->outstandingSendCap) {
	    /* instance is writable, validate the instance is on the ready list. */
	    IocpZapTclNotifier(infoPtr);
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

    if (!initialized) return 0;

    if (mode == TCL_MODE_NONBLOCKING) {
	infoPtr->flags |= IOCP_ASYNC;
    } else {
	infoPtr->flags &= ~(IOCP_ASYNC);
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

    *handlePtr = (ClientData) infoPtr->socket;
    return TCL_OK;
}

static void
IocpCutProc (ClientData instanceData)
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;

    /* Unable to turn off reading, therefore don't notify
     * anyone during the move. */
    infoPtr->tsdHome = NULL;
}

static void
IocpSpliceProc (ClientData instanceData)
{
    ThreadSpecificData *tsdPtr = InitSockets();
    SocketInfo *infoPtr = (SocketInfo *) instanceData;

    infoPtr->tsdHome = tsdPtr;
}

/* =================================================================== */
/* ============== Lo-level buffer and state manipulation ============= */

SocketInfo *
NewSocketInfo (SOCKET socket)
{
    SocketInfo *infoPtr;

    infoPtr = IocpAlloc(sizeof(SocketInfo));
    infoPtr->channel = NULL;
    infoPtr->socket = socket;
    infoPtr->flags = 0;		    /* assume initial blocking state */
    infoPtr->ready = 0;
    infoPtr->outstandingOps = 0;	/* Total operations pending */
    infoPtr->outstandingSends = 0;
    infoPtr->outstandingSendCap = IOCP_SEND_CAP;
    infoPtr->outstandingAccepts = 0;
    infoPtr->outstandingAcceptCap = IOCP_ACCEPT_CAP;
    infoPtr->outstandingRecvs = 0;
    infoPtr->outstandingRecvCap = IOCP_RECV_CAP;
    infoPtr->watchMask = 0;
    infoPtr->readyAccepts = NULL;  
    infoPtr->acceptProc = NULL;
    infoPtr->localAddr = NULL;	    /* Local sockaddr. */
    infoPtr->remoteAddr = NULL;	    /* Remote sockaddr. */
    infoPtr->lastError = NO_ERROR;
    infoPtr->allDone = CreateEvent(NULL, TRUE, FALSE, NULL); /* manual reset */
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

    /* Just in case... */
    if (infoPtr->socket != INVALID_SOCKET) {
	winSock.closesocket(infoPtr->socket);
    }

    if (infoPtr->localAddr) IocpFree(infoPtr->localAddr);
    if (infoPtr->remoteAddr) IocpFree(infoPtr->remoteAddr);

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
    bufPtr = IocpNPPAlloc(sizeof(BufferInfo));
    if (bufPtr == NULL) {
	return NULL;
    }
    /* Allocate the buffer. */
    bufPtr->buf = IocpNPPAlloc(sizeof(BYTE)*buflen);
    if (bufPtr->buf == NULL) {
	IocpNPPFree(bufPtr);
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
    IocpNPPFree(bufPtr->buf);
    IocpNPPFree(bufPtr);
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
    InterlockedExchange(&newInfoPtr->outstandingSendCap,
	    infoPtr->outstandingSendCap);
    InterlockedExchange(&newInfoPtr->outstandingRecvCap,
	    infoPtr->outstandingRecvCap);

    return newInfoPtr;
}

/*
 *  Only zap the notifier when the notifier is waiting and this request
 *  has not already been made previously.
 */

static void
IocpZapTclNotifier (SocketInfo *infoPtr)
{
    /*
     * If we are in the middle of a thread transfer on the channel,
     * infoPtr->tsdHome will be NULL.
     */
    if (infoPtr->tsdHome) {
	EnterCriticalSection(&infoPtr->tsdHome->readySockets->lock);
	if (!InterlockedExchange(&infoPtr->ready, 1)) {
	    /* No entry on the ready list.  Insert one. */
	    IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr,
		    NULL, IOCP_LL_NOLOCK);
	}
	LeaveCriticalSection(&infoPtr->tsdHome->readySockets->lock);
	Tcl_ThreadAlert(infoPtr->tsdHome->threadId);
    }
}

/* takes buffer ownership */
static void
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

    IocpLLPushBack(infoPtr->readyAccepts, acptInfo, &acptInfo->node, 0);

    /*
     * Let IocpCheckProc() know this channel has an accept ready
     * that needs servicing.
     */
    IocpZapTclNotifier(infoPtr);
}

static void
IocpPushRecvAlertToTcl(SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    /* (takes buffer ownership) */
    IocpLLPushBack(infoPtr->llPendingRecv, bufPtr,
	    &bufPtr->node, 0);

    /*
     * Let IocpCheckProc() know this new channel has a ready
     * event (a recv) that needs servicing.
     */
    IocpZapTclNotifier(infoPtr);
}

DWORD
PostOverlappedAccept (SocketInfo *infoPtr, BufferInfo *bufPtr, int useBurst)
{
    DWORD bytes, WSAerr;
    int rc;
    SIZE_T buflen, addr_storage;

    if (infoPtr->flags & IOCP_CLOSING) return WSAENOTCONN;

    /* Recursion limit */
    if (InterlockedIncrement(&infoPtr->outstandingAccepts)
	    > infoPtr->outstandingAcceptCap) {
	InterlockedDecrement(&infoPtr->outstandingAccepts);
	/* Best choice I could think of for an error value. */
	return WSAENOBUFS;
    }

    bufPtr->operation = OP_ACCEPT;
    buflen = bufPtr->buflen;
    addr_storage = infoPtr->proto->addrLen + 16;

    /*
     * Create a ready client socket of the same type for a future
     * incoming connection.
     */

    bufPtr->socket = winSock.WSASocket(infoPtr->proto->af,
	    infoPtr->proto->type, infoPtr->proto->protocol, NULL, 0,
	    WSA_FLAG_OVERLAPPED);

    if (bufPtr->socket == INVALID_SOCKET) {
	return WSAENOTSOCK;
    }

    /*
     * Realloc for the extra needed addr storage space.
     */
    bufPtr->buf = IocpNPPReAlloc(bufPtr->buf, bufPtr->buflen +
	    (addr_storage * 2));
    bufPtr->buflen += (addr_storage * 2);

    /*
     * Increment the outstanding overlapped count for this socket and put
     * the buffer on the pending accepts list.  We need to do this before
     * the operation because it might complete instead of posting.
     */
    
    InterlockedIncrement(&infoPtr->outstandingOps);

    /*
     * Use the special function for overlapped accept() that is provided
     * by the LSP of this socket type.
     */

    rc = infoPtr->proto->AcceptEx(infoPtr->socket, bufPtr->socket,
	    bufPtr->buf, bufPtr->buflen - (addr_storage * 2),
	    addr_storage, addr_storage, &bytes, &bufPtr->ol);

    if (rc == FALSE) {
	if ((WSAerr = winSock.WSAGetLastError()) != WSA_IO_PENDING) {
	    InterlockedDecrement(&infoPtr->outstandingOps);
	    InterlockedDecrement(&infoPtr->outstandingAccepts);
	    bufPtr->WSAerr = WSAerr;
	    return WSAerr;
	}
    } else if (useBurst) {
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
	if (PostOverlappedAccept(infoPtr, newBufPtr, 1) != NO_ERROR) {
	    FreeBufferObj(newBufPtr);
	}
    }

    return NO_ERROR;
}

DWORD
PostOverlappedRecv (SocketInfo *infoPtr, BufferInfo *bufPtr, int useBurst)
{
    WSABUF wbuf;
    DWORD bytes = 0, flags, WSAerr;
    int rc;

    if (infoPtr->flags & IOCP_CLOSING) return WSAENOTCONN;

    /* Recursion limit */
    if (InterlockedIncrement(&infoPtr->outstandingRecvs)
	    > infoPtr->outstandingRecvCap) {
	InterlockedDecrement(&infoPtr->outstandingRecvs);
	/* Best choice I could think of for an error value. */
	return WSAENOBUFS;
    }

    bufPtr->operation = OP_READ;
    wbuf.buf = bufPtr->buf;
    wbuf.len = bufPtr->buflen;
    flags = 0;

    /*
     * Increment the outstanding overlapped count for this socket.
     */

    InterlockedIncrement(&infoPtr->outstandingOps);

    if (infoPtr->proto->type == SOCK_STREAM) {
	rc = winSock.WSARecv(infoPtr->socket, &wbuf, 1, &bytes, &flags,
		&bufPtr->ol, NULL);
    } else {
	rc = winSock.WSARecvFrom(infoPtr->socket, &wbuf, 1, &bytes,
		&flags, bufPtr->addr, &infoPtr->proto->addrLen,
		&bufPtr->ol, NULL);
    }

    /*
     * There are three states that can happen here:
     *
     * 1) WSARecv returns zero when the operation has completed
     *	  immediately and the completion is queued to the port (behind
     *	  us now).
     * 2) WSARecv returns SOCKET_ERROR with WSAGetLastError() returning
     *	  WSA_IO_PENDING to indicate the operation was succesfully
     *	  initiated and will complete at a later time (and possibly
     *	  complete with an error or not).
     * 3) WSARecv returns SOCKET_ERROR with WSAGetLastError() returning
     *	  any other WSAGetLastError() code indicates the operation was
     *	  NOT succesfully initiated and completion will NOT occur.  We
     *	  must feed this error up to Tcl, or it will be lost.
     */

    if (rc == SOCKET_ERROR) {
	if ((WSAerr = winSock.WSAGetLastError()) != WSA_IO_PENDING) {
	    bufPtr->WSAerr = WSAerr;
	    /*
	     * Eventhough we know about the error now, post this to the
	     * port manually.  If we sent this to the revc'd linklist
	     * now, we run the risk of placing this error ahead of good
	     * data waiting in the port behind us (this thread).
	     */
	    PostQueuedCompletionStatus(IocpSubSystem.port, 0,
		(ULONG_PTR) infoPtr, &bufPtr->ol);
	    //return WSAerr;
	    return NO_ERROR;
	}
    } else if (bytes > 0 && useBurst) {
	BufferInfo *newBufPtr;

	/*
	 * The WSARecv(From) has completed now, and was posted to the
	 * port.  Keep giving more WSARecv(From) calls to drain the
	 * internal buffer (AFD.sys).  Why should we wait for the time
	 * when the completion routine is run if we know the connected
	 * socket can take another right now?  IOW, keep recursing until
	 * the WSA_IO_PENDING condition is achieved.
	 * 
	 * The only drawback to this is the amount of outstanding calls
	 * will increase.  There is no method for coming out of a burst
	 * condition to return the count to normal.  This shouldn't be
	 * an issue with short lived sockets -- only ones with a long
	 * lifetime.
	 */

	newBufPtr = GetBufferObj(infoPtr, wbuf.len);
	if (PostOverlappedRecv(infoPtr, newBufPtr, 1 /*useBurst */)
		!= NO_ERROR) {
	    FreeBufferObj(newBufPtr);
	}
    }

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
    InterlockedIncrement(&infoPtr->outstandingOps);
    InterlockedIncrement(&infoPtr->outstandingSends);

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
	    bufPtr->WSAerr = WSAerr;

	    /*
	     * Eventhough we know about the error now, post this to the port
	     * manually, too.  We need to force EOF as the generic layer
	     * needs a little help to know that both sides of our
	     * bidirectional channel are now dead because of this write
	     * side error.
	     */
	    PostQueuedCompletionStatus(IocpSubSystem.port, 0,
		(ULONG_PTR) infoPtr, &bufPtr->ol);
	    //return WSAerr;
	    return NO_ERROR;
	}
    } else {
	/*
	 * The WSASend/To completed now, instead of getting posted.
	 */
	__asm nop;
    }

    return NO_ERROR;
}

static DWORD
PostOverlappedDisconnect (SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    BOOL rc;
    DWORD WSAerr;

    /*
     * Increment the outstanding overlapped count for this socket.
     */
    InterlockedIncrement(&infoPtr->outstandingOps);

    bufPtr->operation = OP_DISCONNECT;

    rc = infoPtr->proto->DisconnectEx(infoPtr->socket, &bufPtr->ol,
	    0 /*TF_REUSE_SOCKET*/, 0);

    if (rc == FALSE) {
	if ((WSAerr = winSock.WSAGetLastError()) != WSA_IO_PENDING) {
	    bufPtr->WSAerr = WSAerr;

	    /*
	     * Eventhough we know about the error now, post this to the port
	     * manually, too.  We need to force EOF as the generic layer
	     * needs a little help to know that both sides of our
	     * bidirectional channel are now dead because of this write
	     * side error.
	     */
	    PostQueuedCompletionStatus(IocpSubSystem.port, 0,
		(ULONG_PTR) infoPtr, &bufPtr->ol);
	    //return WSAerr;
	    return NO_ERROR;
	}
    } else {
	/*
	 * The DisconnectEx completed now, instead of getting posted.
	 */
	__asm nop;
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
    DWORD bytes, flags, WSAerr, error = NO_ERROR;
    BOOL ok;

#if 1
#else
    __try {
#endif
again:
	WSAerr = NO_ERROR;
	flags = 0;

	ok = GetQueuedCompletionStatus(cpinfo->port, &bytes,
		(PULONG_PTR) &infoPtr, &ol, INFINITE);

	if (ok && !infoPtr) {
	    /* A NULL key indicates closure time for this thread. */
#if 1
	    return error;
#else
	    __leave;
#endif
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
	goto again;
#if 1
#else
    }
    __except (error = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
	char msg[50];
	DWORD dummy;
	HANDLE where;
	wsprintf(msg, "Big ERROR!  Completion thread died with %#x.  "
		"You MUST restart the application.  "
		"All socket communication has halted.\n", error);
	OutputDebugString(msg);
	where = GetStdHandle(STD_ERROR_HANDLE);
	if (where && where != INVALID_HANDLE_VALUE) {
	    WriteFile(where, msg, strlen(msg), &dummy, NULL);
	}
    }
#endif

    return error;
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
    register SocketInfo *infoPtr,
    register BufferInfo *bufPtr,
    HANDLE CompPort,
    DWORD bytes,
    DWORD WSAerr,
    DWORD flags)
{
    register SocketInfo *newInfoPtr;
    register BufferInfo *newBufPtr;
    register int i;
    SOCKADDR *local, *remote;
    SIZE_T addr_storage;
    int localLen, remoteLen;


    if (WSAerr == WSA_OPERATION_ABORTED) {
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
	 * Decrement the count of pending accepts from the total.
	 */

	InterlockedDecrement(&infoPtr->outstandingAccepts);

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

	    /* Associate the new socket to our completion port. */
	    CreateIoCompletionPort((HANDLE) newInfoPtr->socket, CompPort,
		    (ULONG_PTR) newInfoPtr, 0);

	    /* post IOCP_INITIAL_RECV_COUNT recvs. */
	    for(i=0; i < IOCP_INITIAL_RECV_COUNT ;i++) {
		newBufPtr = GetBufferObj(newInfoPtr, IOCP_RECV_BUFSIZE);
		if (PostOverlappedRecv(newInfoPtr, newBufPtr, 0)
			!= NO_ERROR) {
		    break;
		}
	    }

	    /* Alert Tcl to this new connection. */
	    IocpAlertToTclNewAccept(infoPtr, newInfoPtr);

	    if (bytes > 0) {
		IocpPushRecvAlertToTcl(newInfoPtr, bufPtr);
	    } else {
		/* No data received from AcceptEx(). */
		FreeBufferObj(bufPtr);
	    }

	} else if (bufPtr->WSAerr == WSA_OPERATION_ABORTED ||
		bufPtr->WSAerr == WSAENOTSOCK) {
	    /* The operation was cancelled and the socket must be closing.
	     * Do NOT replace this returning AcceptEx. */
	    FreeBufferObj(bufPtr);
	    break;

	} else {
	    /*
	     * Possible SYN flood in progress. We WANT this returning
	     * AcceptEx that had an error to be replaced.  Some of the
	     * errors sampled are:
	     *
	     * WSAEHOSTUNREACH, WSAETIMEDOUT, WSAENETUNREACH
	     */
	    FreeBufferObj(bufPtr);
	}
    
	/*
	 * Post another new AcceptEx() to replace this one that just
	 * fired.
	 */

	newBufPtr = GetBufferObj(infoPtr, 0);
	if (PostOverlappedAccept(infoPtr, newBufPtr, 0) != NO_ERROR) {
	    /*
	     * Oh no, the AcceptEx failed.  There is no way to return an
	     * error for this condition.  Tcl has no failure aspect for
	     * listening sockets.
	     */
	    FreeBufferObj(newBufPtr);
	    DbgPrint("\nIOCPsock (non-recoverable): an AcceptEx call failed and was not replaced.\n");
	}
	break;

    case OP_READ:

	/* Decrement the count of pending recvs from the total. */
	InterlockedDecrement(&infoPtr->outstandingRecvs);

	if (bytes > 0) {
	    if (infoPtr->outstandingRecvCap > 1) {
		/*
		 * Create a new buffer object to replace the one that just
		 * came in, but use a hard-coded size for now until a
		 * method to control the receive buffer size exists.
		 *
		 * TODO: make an fconfigure for this and store it in the
		 * SocketInfo struct.
		 */

		newBufPtr = GetBufferObj(infoPtr, IOCP_RECV_BUFSIZE);
		if (PostOverlappedRecv(infoPtr, newBufPtr, 1) != NO_ERROR) {
		    FreeBufferObj(newBufPtr);
		}
	    }
	} else if (infoPtr->flags & IOCP_CLOSING && infoPtr->flags & IOCP_ASYNC) {
	    infoPtr->flags |= IOCP_CLOSABLE;
	    FreeBufferObj(bufPtr);
	    break;
	}

	/* Takes buffer ownership. */
	IocpPushRecvAlertToTcl(infoPtr, bufPtr);

	break;

    case OP_WRITE:

	/*
	 * Decrement the count of pending sends from the total.
	 */

	InterlockedDecrement(&infoPtr->outstandingSends);

	if (infoPtr->flags & IOCP_CLOSING) {
	    FreeBufferObj(bufPtr);
	    break;
	}

	if (!(infoPtr->flags & IOCP_CLOSING)) {
	    if (bufPtr->WSAerr != NO_ERROR && infoPtr->llPendingRecv) {
		newBufPtr = GetBufferObj(infoPtr, 0);
		newBufPtr->WSAerr = bufPtr->WSAerr;
		/* Force EOF on the read side, too, for a write side error. */
		IocpPushRecvAlertToTcl(infoPtr, newBufPtr);
	    } else if (infoPtr->watchMask & TCL_WRITABLE) {
		/* can write more. */
		IocpZapTclNotifier(infoPtr);
	    }
	}
	FreeBufferObj(bufPtr);
	break;

    case OP_CONNECT:

	infoPtr->llPendingRecv = IocpLLCreate();

	if (bufPtr->WSAerr != NO_ERROR) {
	    infoPtr->lastError = bufPtr->WSAerr;
	    newBufPtr = GetBufferObj(infoPtr, 0);
	    newBufPtr->WSAerr = bufPtr->WSAerr;
	    /* Force EOF. */
	    IocpPushRecvAlertToTcl(infoPtr, newBufPtr);
	} else {
	    winSock.setsockopt(infoPtr->socket, SOL_SOCKET,
		    SO_UPDATE_CONNECT_CONTEXT, NULL, 0);

	    /* post IOCP_INITIAL_RECV_COUNT recvs. */
	    for(i=0; i < IOCP_INITIAL_RECV_COUNT ;i++) {
		newBufPtr = GetBufferObj(infoPtr, IOCP_RECV_BUFSIZE);
		if (PostOverlappedRecv(infoPtr, newBufPtr, 0) != NO_ERROR) {
		    FreeBufferObj(bufPtr);
		    break;
		}
	    }
	    IocpZapTclNotifier(infoPtr);
	}
	FreeBufferObj(bufPtr);
	break;

    case OP_DISCONNECT:
	infoPtr->flags |= IOCP_CLOSABLE;
	FreeBufferObj(bufPtr);
	break;

    case OP_TRANSMIT:
    case OP_LOOKUP:
    case OP_IOCTL:
	/* For future use. */
	break;
    }

done:
    if (InterlockedDecrement(&infoPtr->outstandingOps) <= 0
	    && infoPtr->flags & IOCP_CLOSING) {
	/* This is the last operation. */
	if (infoPtr->flags & IOCP_ASYNC && infoPtr->flags & IOCP_CLOSABLE) {
	    FreeSocketInfo(infoPtr);
	} else {
	    SetEvent(infoPtr->allDone);
	}
    }
}


/* =================================================================== */
/* ====================== Private memory heap ======================== */

/* general pool */

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

/* special pool */

__inline LPVOID
IocpNPPAlloc (SIZE_T size)
{
    LPVOID p;
    p = HeapAlloc(IocpSubSystem.NPPheap, HEAP_ZERO_MEMORY, size);
    return p;
}

__inline LPVOID
IocpNPPReAlloc (LPVOID block, SIZE_T size)
{
    LPVOID p;
    p = HeapReAlloc(IocpSubSystem.NPPheap, HEAP_ZERO_MEMORY, block, size);
    return p;
}

__inline BOOL
IocpNPPFree (LPVOID block)
{
    return HeapFree(IocpSubSystem.NPPheap, 0, block);
}


/* =================================================================== */
/* =================== Protocol neutral procedures =================== */

/*
 *-----------------------------------------------------------------------
 *
 * IocpInitProtocolData --
 *
 *	This function initializes the WS2ProtocolData struct.
 *
 * Results:
 *	nothing.
 *
 * Side effects:
 *	Fills in the WS2ProtocolData structure, if uninitialized.
 *
 *-----------------------------------------------------------------------
 */

void
IocpInitProtocolData (SOCKET sock, WS2ProtocolData *pdata)
{
    DWORD bytes;

    /* is it already cached? */
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
	    /* Use our lame Win2K/NT4 emulation for this. */
	    pdata->ConnectEx = OurConnectEx;
	}
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&DisconnectExGuid, sizeof(GUID),
		&pdata->DisconnectEx,
		sizeof(pdata->DisconnectEx),
		&bytes, NULL, NULL);
	if (pdata->DisconnectEx == NULL) {
	    /* Use our lame Win2K/NT4 emulation for this. */
	    pdata->DisconnectEx = OurDisonnectEx;
	}
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&TransmitFileGuid, sizeof(GUID),
		&pdata->TransmitFile,
		sizeof(pdata->TransmitFile),
		&bytes, NULL, NULL);
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&TransmitPacketsGuid, sizeof(GUID),
		&pdata->TransmitPackets,
		sizeof(pdata->TransmitPackets),
		&bytes, NULL, NULL);
	if (pdata->TransmitPackets == NULL) {
	    /* There is no Win2K/NT4 emulation for this. */
	    pdata->TransmitPackets = NULL;
	}
        winSock.WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&WSARecvMsgGuid, sizeof(GUID),
		&pdata->WSARecvMsg,
		sizeof(pdata->WSARecvMsg),
		&bytes, NULL, NULL);
	if (pdata->WSARecvMsg == NULL) {
	    /* There is no Win2K/NT4 emulation for this. */
	    pdata->WSARecvMsg = NULL;
	}
    }
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
	/* an older platSDK needed this; the current doesn't.
	winSock.WSASetLastError(result); */
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
    ENOTCONN,		/* WSAEDISCON		    Returned by WSARecv or WSARecvFrom to indicate the remote party has initiated a graceful shutdown sequence. */
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

BOOL PASCAL
OurDisonnectEx (
    SOCKET hSocket,
    LPOVERLAPPED lpOverlapped,
    DWORD dwFlags,
    DWORD reserved)
{
    BufferInfo *bufPtr;
    bufPtr = CONTAINING_RECORD(lpOverlapped, BufferInfo, ol);
    winSock.WSASendDisconnect(hSocket, NULL);
    PostQueuedCompletionStatus(IocpSubSystem.port, 0,
	    (ULONG_PTR) bufPtr->parent, lpOverlapped);
    return TRUE;
}

