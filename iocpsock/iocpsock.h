/* ----------------------------------------------------------------------
 *
 * iocpsock.h --
 *
 *	Main header file for the shared stuff.
 *
 * ----------------------------------------------------------------------
 * RCS: @(#) $Id$
 * ----------------------------------------------------------------------
 */

#ifndef INCL_iocpsock_h_
#define INCL_iocpsock_h_

#define IOCPSOCK_MAJOR_VERSION   2
#define IOCPSOCK_MINOR_VERSION   0
#define IOCPSOCK_RELEASE_LEVEL   TCL_FINAL_RELEASE
#define IOCPSOCK_RELEASE_SERIAL  0

#define IOCPSOCK_VERSION	"2.0"
#define IOCPSOCK_PATCH_LEVEL	"2.0.0"


#ifndef RC_INVOKED

/* Enables NT5 special features. */
#define _WIN32_WINNT 0x0500

/* Turn on function typedefs. */
#define INCL_WINSOCK_API_TYPEDEFS   1

/* Turn off prototypes. */
#define INCL_WINSOCK_API_PROTOTYPES 0

/* winsock2.h should check for the intrinsic _WIN32, but doesn't. */
#ifndef WIN32
#define WIN32
#endif

/* wspiapi.h can't handle ws2 typedefs only, so we'll exclude it
 * from the include chain. */
#define _WSPIAPI_H_

#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef IPPROTO_IPV6
#include <tpipv6.h> /* For IPv6 Tech Preview. */
#endif

/* probably not needed */
//#include <iphlpapi.h>

/* Microsoft specific extensions to WS2 */
#include <mswsock.h>

/* namespace and service resolution */
#include <svcguid.h>
#include <nspapi.h>

#if defined(_DEBUG) && defined(SHOWDBG)
#   define DbgPrint(msg) OutputDebugString(msg);
#elif defined(SHOWDBG)
#   define DbgPrint(msg) \
	    { \
		DWORD dummy; \
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, strlen(msg), &dummy, NULL); \
	    }
#else
#   define DbgPrint(msg)
#endif



typedef struct {
    HMODULE	hModule;	/* Handle to the WinSock DLL. */
    WORD	wVersionLoaded;	/* Winsock API interface loaded. */

    /* Winsock 1.1 functions */
    LPFN_ACCEPT		    accept;
    LPFN_BIND		    bind;
    LPFN_CLOSESOCKET	    closesocket;
    LPFN_CONNECT	    connect;
    LPFN_GETHOSTBYADDR	    gethostbyaddr;
    LPFN_GETHOSTBYNAME	    gethostbyname;
    LPFN_GETHOSTNAME	    gethostname;
    LPFN_GETPEERNAME	    getpeername;
    LPFN_GETSERVBYNAME	    getservbyname;
    LPFN_GETSERVBYPORT	    getservbyport;
    LPFN_GETSOCKNAME	    getsockname;
    LPFN_GETSOCKOPT	    getsockopt;
    LPFN_HTONL		    htonl;
    LPFN_HTONS		    htons;
    LPFN_INET_ADDR	    inet_addr;
    LPFN_INET_NTOA	    inet_ntoa;
    LPFN_IOCTLSOCKET	    ioctlsocket;
    LPFN_LISTEN		    listen;
    LPFN_NTOHS		    ntohs;
    LPFN_RECV		    recv;
    LPFN_RECVFROM	    recvfrom;
    LPFN_SELECT		    select;
    LPFN_SEND		    send;
    LPFN_SENDTO		    sendto;
    LPFN_SETSOCKOPT	    setsockopt;
    LPFN_SHUTDOWN	    shutdown;
    LPFN_SOCKET		    socket;
    LPFN_WSAASYNCSELECT	    WSAAsyncSelect;
    LPFN_WSACLEANUP	    WSACleanup;
    LPFN_WSAGETLASTERROR    WSAGetLastError;
    LPFN_WSASETLASTERROR    WSASetLastError;
    LPFN_WSASTARTUP	    WSAStartup;

    /* Winsock 2.2 functions */
    LPFN_WSAACCEPT	    WSAAccept;
    LPFN_WSAADDRESSTOSTRINGA	WSAAddressToString;
    LPFN_WSACLOSEEVENT	    WSACloseEvent;
    LPFN_WSACONNECT	    WSAConnect;
    LPFN_WSACREATEEVENT	    WSACreateEvent;
    LPFN_WSADUPLICATESOCKETA	WSADuplicateSocket;
    LPFN_WSAENUMNAMESPACEPROVIDERSA WSAEnumNameSpaceProviders;
    LPFN_WSAENUMNETWORKEVENTS	WSAEnumNetworkEvents;
    LPFN_WSAENUMPROTOCOLSA   WSAEnumProtocols;
    LPFN_WSAEVENTSELECT	    WSAEventSelect;
    LPFN_WSAGETOVERLAPPEDRESULT	WSAGetOverlappedResult;
    LPFN_WSAGETQOSBYNAME    WSAGetQOSByName;
    LPFN_WSAGETSERVICECLASSINFO	WSAGetServiceClassInfo;
    LPFN_WSAGETSERVICECLASSNAMEBYCLASSIDA WSAGetServiceClassNameByClassId;
    LPFN_WSAHTONL	    WSAHtonl;
    LPFN_WSAHTONS	    WSAHtons;
    LPFN_WSAINSTALLSERVICECLASSA WSAInstallServiceClass;
    LPFN_WSAIOCTL	    WSAIoctl;
    LPFN_WSAJOINLEAF	    WSAJoinLeaf;
    LPFN_WSALOOKUPSERVICEBEGINA	WSALookupServiceBegin;
    LPFN_WSALOOKUPSERVICEEND	WSALookupServiceEnd;
    LPFN_WSALOOKUPSERVICENEXTA	WSALookupServiceNext;
    LPFN_WSANSPIOCTL	    WSANSPIoctl;
    LPFN_WSANTOHL	    WSANtohl;
    LPFN_WSANTOHS	    WSANtohs;
    LPFN_WSAPROVIDERCONFIGCHANGE WSAProviderConfigChange;
    LPFN_WSARECV	    WSARecv;
    LPFN_WSARECVDISCONNECT  WSARecvDisconnect;
    LPFN_WSARECVFROM	    WSARecvFrom;
    LPFN_WSAREMOVESERVICECLASS	WSARemoveServiceClass;
    LPFN_WSARESETEVENT	    WSAResetEvent;
    LPFN_WSASEND	    WSASend;
    LPFN_WSASENDDISCONNECT  WSASendDisconnect;
    LPFN_WSASENDTO	    WSASendTo;
    LPFN_WSASETEVENT	    WSASetEvent;
    LPFN_WSASETSERVICEA	    WSASetService;
    LPFN_WSASOCKETA	    WSASocket;
    LPFN_WSASTRINGTOADDRESSA	WSAStringToAddress;
    LPFN_WSAWAITFORMULTIPLEEVENTS WSAWaitForMultipleEvents;

} WinsockProcs;

extern WinsockProcs winSock;
extern int initialized;
extern HMODULE iocpModule;

/* wspiapi.h doesn't like typedefs, so fix it. */
#define inet_addr	winSock.inet_addr
#define gethostbyname   winSock.gethostbyname
#define WSAGetLastError winSock.WSAGetLastError
#define WSASetLastError winSock.WSASetLastError
#define htons		winSock.htons
#define getservbyname   winSock.getservbyname
#define htonl		winSock.htonl
#define inet_ntoa	winSock.inet_ntoa
#define ntohs		winSock.ntohs
#define getservbyport	winSock.getservbyport
#define gethostbyaddr	winSock.gethostbyaddr
#undef _WSPIAPI_H_
#include <wspiapi.h>
#undef inet_addr
#undef gethostbyname
#undef WSAGetLastError
#undef WSASetLastError
#undef htons
#undef getservbyname
#undef htonl
#undef inet_ntoa
#undef ntohs
#undef getservbyport
#undef gethostbyaddr


/*
 * Specific protocol information is stored here and shared to all
 * SocketInfo objects of that type.
 */
typedef struct {
    int af;		    /* Address family. */
    int	type;		    /* Address type. */
    int	protocol;	    /* protocol type. */
    size_t addrLen;	    /* length of protocol specific SOCKADDR */

    /* LSP specific extension functions */
    LPFN_ACCEPTEX		AcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS	GetAcceptExSockaddrs;
    LPFN_CONNECTEX		ConnectEx;
    LPFN_DISCONNECTEX		DisconnectEx;
    LPFN_TRANSMITFILE		TransmitFile;
    /* The only caveat of using this TransmitFile extension API is that
       on Windows NT Workstation or Windows 2000 Professional only two
       requests will be processed at a time. You must be running on
       Windows NT or Windows 2000 Server, Windows 2000 Advanced Server,
       or Windows 2000 Data Center to get full usage of this specialized
       API. */
    LPFN_TRANSMITPACKETS	TransmitPackets;
    LPFN_WSARECVMSG		WSARecvMsg;
} WS2ProtocolData;

/* Linked-List node object. */
struct _ListNode;
struct _List;
typedef struct _ListNode {
    struct _ListNode *next;	/* node in back */
    struct _ListNode *prev;	/* node in back */
    struct _List *ll;		/* parent linked-list */
    LPVOID lpItem;		/* storage item */
} LLNODE, *LPLLNODE;

/* Linked-List object. */
typedef struct _List {
    struct _ListNode *front;	/* head of list */
    struct _ListNode *back;	/* tail of list */
    LONG lCount;		/* nodes contained */
    CRITICAL_SECTION lock;	/* accessor lock */
    HANDLE haveData;		/* event when data is added to an empty list */
} LLIST, *LPLLIST;


struct SocketInfo;

/*
 * This is our per I/O buffer. It contains a WSAOVERLAPPED structure as well
 * as other necessary information for handling an IO operation on a socket.
 */
typedef struct _BufferInfo {
    WSAOVERLAPPED ol;
    struct SocketInfo *parent;
    SOCKET socket;	    /* Used for AcceptEx client socket */
    DWORD WSAerr;	    /* Any error that occured for this operation. */
    BYTE *buf;		    /* Buffer for recv/send/AcceptEx */
    SIZE_T buflen;	    /* Length of the buffer */
    SIZE_T used;	    /* Length of the buffer used (post operation) */
#   define OP_ACCEPT	0   /* AcceptEx() */
#   define OP_READ	1   /* WSARecv()/WSARecvFrom() */
#   define OP_WRITE	2   /* WSASend()/WSASendTo() */
#   define OP_CONNECT	3   /* ConnectEx() */
#   define OP_DISCONNECT 4  /* DisconnectEx() */
#   define OP_TRANSMIT	5   /* TransmitFile() */
#   define OP_IOCTL	6   /* WSAIoctl() */
#   define OP_LOOKUP	7   /* TODO: For future use, WSANSIoctl()?? */
    int operation;	    /* Type of operation issued */
    LPSOCKADDR addr;	    /* addr storage space for WSARecvFrom/WSASendTo. */
    LLNODE node;	    /* linked list node */
} BufferInfo;

#define __WIN32__
#include "tclPort.h"

/* We need at least the Tcl_Obj interface that was started in 8.0 */
#if TCL_MAJOR_VERSION < 8
#   error "we need Tcl 8.0 or greater to build this"

/* Check for Stubs compatibility when asked for it. */
#elif defined(USE_TCL_STUBS) && TCL_MAJOR_VERSION == 8 && \
	(TCL_MINOR_VERSION == 0 || \
        (TCL_MINOR_VERSION == 1 && TCL_RELEASE_LEVEL != TCL_FINAL_RELEASE))
#   error "Stubs interface doesn't work in 8.0 and alpha/beta 8.1; only 8.1.0+"
#endif

/* tclWinPort.h sets these -- remove them */
#undef getservbyname
#undef getsockopt
#undef ntohs
#undef setsockopt


typedef struct ThreadSpecificData {
    Tcl_ThreadId threadId;
    LPLLIST readySockets;
} ThreadSpecificData;

extern Tcl_ThreadDataKey dataKey;

/*
 * The following structure is used to store the data associated with
 * each socket.
 */

#define IOCP_EOF	    (1<<0)
#define IOCP_CLOSING	    (1<<1)
#define IOCP_ASYNC	    (1<<2)
#define IOCP_CLOSABLE	    (1<<3)

#pragma pack (push, 4)

typedef struct SocketInfo {
    Tcl_Channel channel;	    /* Tcl channel for this socket. */
    SOCKET socket;		    /* Windows SOCKET handle. */
    DWORD flags;		    /* info about this socket. */

    /* we need 32-bit alignment for these: */
    volatile LONG markedReady;	    /* indicates we are on the readySockets list.
				     * Access must be protected within the
				     * tsdPtr->readySockets->lock critical
				     * section. */
    volatile LONG outstandingOps;   /* Count of all overlapped operations. */
    volatile LONG outstandingSends; /* Count of overlapped WSASend() operations. */
    volatile LONG outstandingSendCap; /* Limit of outstanding overlapped WSASend
				     * operations allowed. */
    volatile LONG outstandingAccepts; /* Count of overlapped AcceptEx() operations. */
    volatile LONG outstandingAcceptCap; /* Limit of outstanding overlapped AcceptEx
				     * operations allowed. */
    volatile LONG outstandingRecvs; /* Count of overlapped WSARecv() operations. */
    volatile LONG outstandingRecvCap; /* Limit of outstanding overlapped WSARecv
				     * operations allowed. */

    int watchMask;		    /* events we are interested in. */
    WS2ProtocolData *proto;	    /* Network protocol info. */
    ThreadSpecificData *tsdHome;    /* TSD block for getting back to our
				     * origin. */
    /* For listening sockets: */
    LPLLIST readyAccepts;	    /* Ready accepts() in queue. */
    Tcl_TcpAcceptProc *acceptProc;  /* Proc to call on accept. */
    ClientData acceptProcData;	    /* The data for the accept proc. */

    /* Neutral SOCKADDR data: */
    LPSOCKADDR localAddr;	    /* Local sockaddr. */
    LPSOCKADDR remoteAddr;	    /* Remote sockaddr. */

    DWORD lastError;		    /* Error code from last operation. */
    HANDLE allDone;		    /* manual reset event */
    LPLLIST llPendingRecv;	    /* Our pending recv list. */

} SocketInfo;

#pragma pack (pop)

typedef struct IocpAcceptInfo {
    SOCKADDR_STORAGE local;
    int localLen;
    SOCKADDR_STORAGE remote;
    int remoteLen;
    SocketInfo *clientInfo;
    LLNODE node;
} IocpAcceptInfo;

extern Tcl_ChannelType IocpChannelType;

/*
 * Only one subsystem is ever created, but we group it within a
 * struct just to be organized.
 */
typedef struct CompletionPortInfo {
    HANDLE port;	    /* The completion port handle. */
    HANDLE heap;	    /* General private heap used for objects 
			     * that don't need to interact with Tcl
			     * directly. */
    HANDLE NPPheap;	    /* Special private heap for all data that
			     * will be set with special attributes for
			     * the non-paged pool (WSABUF and OVERLAPPED)
			     */
    HANDLE thread;	    /* The single threads for handling the
			     * completion routine. */
} CompletionPortInfo;

extern CompletionPortInfo IocpSubSystem;

TCL_DECLARE_MUTEX(initLock)

extern ThreadSpecificData *InitSockets();
extern void		IocpInitProtocolData (SOCKET sock, WS2ProtocolData *pdata);
extern int		CreateSocketAddress (const char *addr,
			    const char *port, LPADDRINFO inhints,
			    LPADDRINFO *result);
extern void		FreeSocketAddress(LPADDRINFO addrinfo);
extern Tcl_Channel	Iocp_OpenTcpClient (Tcl_Interp *interp,
			    CONST char *port, CONST char *host,
			    CONST char *myaddr, CONST char *myport,
			    int async);
extern Tcl_Channel	Iocp_OpenTcpServer (Tcl_Interp *interp,
			    CONST char *port, CONST char *host,
			    Tcl_TcpAcceptProc *acceptProc,
			    ClientData acceptProcData);
extern DWORD		PostOverlappedAccept (SocketInfo *infoPtr,
			    BufferInfo *acceptobj, int useBurst);
extern DWORD		PostOverlappedRecv (SocketInfo *infoPtr,
			    BufferInfo *recvobj, int useBurst);
extern void		IocpWinConvertWSAError(DWORD errCode);
extern void		FreeBufferObj(BufferInfo *obj);

extern BufferInfo *	GetBufferObj (SocketInfo *infoPtr,
			    SIZE_T buflen);
extern SocketInfo *	NewSocketInfo (SOCKET socket);
extern void		FreeSocketInfo (SocketInfo *infoPtr);
extern int		HasSockets (Tcl_Interp *interp);
extern char *		GetSysMsg (DWORD id);
extern Tcl_Obj *	GetSysMsgObj (DWORD id);
extern Tcl_ObjCmdProc	Iocp_SocketObjCmd;

/* private memory stuff */
extern __inline LPVOID	IocpAlloc (SIZE_T size);
extern __inline LPVOID  IocpReAlloc (LPVOID block, SIZE_T size);
extern __inline BOOL	IocpFree (LPVOID block);
extern __inline LPVOID	IocpNPPAlloc (SIZE_T size);
extern __inline LPVOID  IocpNPPReAlloc (LPVOID block, SIZE_T size);
extern __inline BOOL	IocpNPPFree (LPVOID block);


/* Thread safe linked-list management. */

/* state bitmask. */
#define IOCP_LL_NOLOCK		(1<<0)
#define IOCP_LL_NODESTROY	(1<<1)

extern LPLLIST		IocpLLCreate ();
extern BOOL		IocpLLDestroy (LPLLIST ll);
extern LPLLNODE		IocpLLPushBack (LPLLIST ll, LPVOID lpItem,
				LPLLNODE pnode, DWORD dwState);
extern LPLLNODE		IocpLLPushFront (LPLLIST ll, LPVOID lpItem,
				LPLLNODE pnode, DWORD dwState);
extern BOOL		IocpLLPop (LPLLNODE pnode, DWORD dwState);
extern BOOL		IocpLLPopAll (LPLLIST ll, LPLLNODE snode,
				DWORD dwState);
extern BOOL		IocpLLPopAllCompare (LPLLIST ll, LPVOID lpItem,
				DWORD dwState);
extern LPVOID		IocpLLPopBack (LPLLIST ll, DWORD dwState,
				DWORD timeout);
extern LPVOID		IocpLLPopFront (LPLLIST ll, DWORD dwState,
				DWORD timeout);
extern BOOL		IocpLLIsNotEmpty (LPLLIST ll);
extern BOOL		IocpLLNodeDestroy (LPLLNODE node);
extern SIZE_T		IocpLLGetCount (LPLLIST ll);

/*
 * Error code management.  The first four are missing from the core
 * which is why I prefix them with Tcl_ .
 */
extern CONST char *	Tcl_Win32ErrId (DWORD errorCode);
extern CONST char *	Tcl_Win32ErrMsg TCL_VARARGS_DEF(DWORD, arg1);
extern CONST char *	Tcl_Win32ErrMsgVA (DWORD errorCode, va_list argList);
extern CONST char *	Tcl_Win32Error TCL_VARARGS_DEF(Tcl_Interp *, arg1);
extern CONST char *	IocpErrId (DWORD errorCode);
extern CONST char *	IocpErrMsg TCL_VARARGS_DEF(DWORD, arg1);
extern CONST char *	IocpErrMsgVA (DWORD errorCode, va_list argList);
extern CONST char *	IocpError TCL_VARARGS_DEF(Tcl_Interp *, arg1);

/*
 * ----------------------------------------------------------------------
 * Some stuff that needs to be switches or fconfigures, but aren't yet.
 * ----------------------------------------------------------------------
 */

/*
 * Default number of overlapped AcceptEx calls to place on a new
 * listening socket.  Base minimum allowed, found by experimentation.
 * This is the base pool count.  Don't go below this number or the
 * listening socket will start returning errors quite easily.
 *
 * Use the -backlog fconfigure on the listening socket to set the
 * pool size.  Each overlapped AcceptEx call will reserve ~500 bytes
 * of the non-paged memory pool.  Larger IS better, if you don't mind
 * the memory in reserve.  Choose a good sized -backlog such as 500
 * if you want it "bunker buster proof".  The NP pool is a global
 * resource for all processes and is said to have a limit around 1/4 of
 * the physical memory.  500 overlapped AccepEx calls * ~500 bytes = ~250K
 * of reserved NP pool memory.  Only use such high -backlog sizes
 * for VERY heavy load servers that you want to handle SYN attacks
 * gracefully.
 */

/* This is the -backlog fconfigure's default value. */
#define IOCP_ACCEPT_CAP		    5

/*
 * We do not want an initial recv() with a new connection.  Use of this
 * feature would require a watchdog thread for doing clean-up of mid-state
 * (connected, but no data yet) connections and is just asking to be a DoS
 * hole..  Not only that, but only works on protocols where the client
 * speaks first.  HTTP may be one, but many others are not.
 *
 *  !DO NOT USE THIS FEATURE!  Turn it off.
 */
#define IOCP_ACCEPT_BUFSIZE	    0

/*
 * Initial count of how many WSARecv(From) calls to place on a connected
 * socket.  The actual count can grow automatically based on burst
 * activity to the cap count (See the recursion used in PostOverlappedRecv
 * for details).
 */

#define IOCP_INITIAL_RECV_COUNT	    1
/* This is the -recvburst fconfigure's default value. */
#define IOCP_RECV_CAP		    20

/*
 * How large do we want the receive buffers?  Use multiples of the
 * page size only (4096) as windows will lock this on page boundaries
 * anyways.
 */

#define IOCP_RECV_BUFSIZE	    4096

/*
 * Initial (default) cap on send concurrency.  This is the -sendcap
 * fconfigure's default value.
 */

#define IOCP_SEND_CAP		    20


#endif  /* #ifndef RC_INVOKED */
#endif /* #ifndef INCL_iocpsock_h_ */
