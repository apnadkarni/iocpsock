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
    LPFN_WSAADDRESSTOSTRINGA	WSAAddressToStringA;
    LPFN_WSACLOSEEVENT	    WSACloseEvent;
    LPFN_WSACONNECT	    WSAConnect;
    LPFN_WSACREATEEVENT	    WSACreateEvent;
    LPFN_WSADUPLICATESOCKETA	WSADuplicateSocketA;
    LPFN_WSAENUMNAMESPACEPROVIDERSA WSAEnumNameSpaceProvidersA;
    LPFN_WSAENUMNETWORKEVENTS	WSAEnumNetworkEvents;
    LPFN_WSAENUMPROTOCOLSA   WSAEnumProtocolsA;
    LPFN_WSAEVENTSELECT	    WSAEventSelect;
    LPFN_WSAGETOVERLAPPEDRESULT	WSAGetOverlappedResult;
    LPFN_WSAGETQOSBYNAME    WSAGetQOSByName;
    LPFN_WSAHTONL	    WSAHtonl;
    LPFN_WSAHTONS	    WSAHtons;
    LPFN_WSAIOCTL	    WSAIoctl;
    LPFN_WSAJOINLEAF	    WSAJoinLeaf;
    LPFN_WSANTOHL	    WSANtohl;
    LPFN_WSANTOHS	    WSANtohs;
    LPFN_WSAPROVIDERCONFIGCHANGE WSAProviderConfigChange;
    LPFN_WSARECV	    WSARecv;
    LPFN_WSARECVDISCONNECT  WSARecvDisconnect;
    LPFN_WSARECVFROM	    WSARecvFrom;
    LPFN_WSARESETEVENT	    WSAResetEvent;
    LPFN_WSASEND	    WSASend;
    LPFN_WSASENDDISCONNECT  WSASendDisconnect;
    LPFN_WSASENDTO	    WSASendTo;
    LPFN_WSASETEVENT	    WSASetEvent;
    LPFN_WSASOCKETA	    WSASocketA;
    LPFN_WSASTRINGTOADDRESSA	WSAStringToAddressA;
    LPFN_WSAWAITFORMULTIPLEEVENTS WSAWaitForMultipleEvents;

} WinsockProcs;

extern WinsockProcs winSock;
extern int initialized;

/* wspiapi.h doesn't like typedefs, so fix it. */
#define inet_addr	winSock.inet_addr
#define gethostbyname   winSock.gethostbyname
#define WSAGetLastError winSock.WSAGetLastError
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
    LPFN_ACCEPTEX		AcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS	GetAcceptExSockaddrs;
    LPFN_CONNECTEX		ConnectEx;

} WS2ProtocolData;

/*
 * GUIDs used for the WSAIoctl call to get the function address from the
 * LSP.
 */
extern GUID gAcceptExGuid;		/* AcceptEx() */
extern GUID gGetAcceptExSockaddrsGuid;	/* GetAcceptExSockaddrs() */
extern GUID gConnectExGuid;		/* ConnectEx() */

/* Linked-List node object. */
struct _ListNode;
struct _List;
typedef struct _ListNode {
    struct _ListNode *next;	/* node in front */
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
#   define OP_CONNECT	3   /* ConnectEx */
#   define OP_LOOKUP	4   /* TODO: For future use */
    int operation;	    /* Type of operation issued */
    LPSOCKADDR addr;	    /* addr storage space for WSARecvFrom/WSASendTo. */
    ULONG IoOrder;	    /* Order in which this I/O was posted */
    LLNODE node;	    /* linked list node */
} BufferInfo;


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
#define IOCP_BLOCKING	    (1<<2)

typedef struct SocketInfo {
    Tcl_Channel channel;	    /* Tcl channel for this socket. */
    SOCKET socket;		    /* Windows SOCKET handle. */
    DWORD flags;
    WS2ProtocolData *proto;	    /* Network protocol info. */
    CRITICAL_SECTION critSec;	    /* Accessor lock. */
    ThreadSpecificData *tsdHome;    /* TSD block for getting back to our
				     * origin. */
    /* For listening sockets: */
    LPLLIST readyAccepts;	    /* Ready accepts() in queue. */
    LPLLIST llPendingAccepts;	    /* List of pending accepts(). */
    Tcl_TcpAcceptProc *acceptProc;  /* Proc to call on accept. */
    ClientData acceptProcData;	    /* The data for the accept proc. */

    /* Neutral SOCKADDR data: */
    LPSOCKADDR localAddr;	    /* Local sockaddr. */
    LPSOCKADDR remoteAddr;	    /* Remote sockaddr. */

    int watchMask;		    /* Tcl events we are interested in. */
    int readyMask;
    DWORD lastError;		    /* Error code from last message. */
    DWORD writeError;		    /* Error code from last Send(To). */
    short outstandingSends;
    short maxOutstandingSends;
    volatile LONG OutstandingOps;	    
//    ULONG LastSendIssued; // Last sequence number sent
//    ULONG IoCountIssued;
    LPLLIST llPendingRecv;	    /* Our pending recv list. */
    LLNODE node;

} SocketInfo;

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
    HANDLE heap;	    /* Private heap used for WSABUFs and other
			     * objects that don't need to interact with
			     * Tcl directly. */
#   define MAX_COMPLETION_THREAD_COUNT	16
			    /* Maximum number of completion threads
			     * allowed. */
    HANDLE threads[MAX_COMPLETION_THREAD_COUNT];
			    /* The array of threads for handling the
			     * completion routines. */
    HANDLE stop;	    /* stop event */
    HANDLE watchDogThread;  /* Used for cleaning up halfway accepted sockets */
    LPLLIST listeningSockets;  /* list for where we store all listening sockets. */

} CompletionPortInfo;

extern CompletionPortInfo IocpSubSystem;

TCL_DECLARE_MUTEX(initLock)

extern int	    CreateSocketAddress (const char *addr,
			    const char *port, LPADDRINFO inhints,
			    LPADDRINFO *result);
extern void	    FreeSocketAddress(LPADDRINFO addrinfo);
extern Tcl_Channel  Iocp_OpenTcpClient (Tcl_Interp *interp,
			    CONST char *port, CONST char *host,
			    CONST char *myaddr, CONST char *myport,
			    int async);
extern Tcl_Channel  Iocp_OpenTcpServer (Tcl_Interp *interp,
			    CONST char *port, CONST char *host,
			    Tcl_TcpAcceptProc *acceptProc,
			    ClientData acceptProcData);
extern DWORD	    PostOverlappedAccept (SocketInfo *infoPtr,
			    BufferInfo *acceptobj);
extern void	    HandleIo(SocketInfo *infoPtr, BufferInfo *bufPtr,
			    HANDLE compPort, DWORD bytes, DWORD error,
			    DWORD flags);
extern void	    IocpWinConvertWSAError(DWORD errCode);
extern void	    FreeBufferObj(BufferInfo *obj);

extern BufferInfo *	GetBufferObj (SocketInfo *infoPtr, SIZE_T buflen);
extern SocketInfo *	NewSocketInfo (SOCKET socket);
extern void		FreeSocketInfo (SocketInfo *infoPtr);
extern int		HasSockets (Tcl_Interp *interp);
extern char *		GetSysMsg (DWORD id);
extern Tcl_Obj *	GetSysMsgObj (DWORD id);
extern Tcl_ObjCmdProc	Iocp_SocketObjCmd;
extern __inline LPVOID	IocpAlloc (SIZE_T size);
extern __inline LPVOID  IocpReAlloc (LPVOID block, SIZE_T size);
extern __inline BOOL	IocpFree (LPVOID block);


/* Thread safe linked-list management. */

/* state bitmask. */
#define IOCP_LL_NOLOCK		(1<<0)
#define IOCP_LL_NODESTROY	(1<<1)

extern LPLLIST		IocpLLCreate ();
extern BOOL		IocpLLDestroy (LPLLIST ll);
extern LPLLNODE		IocpLLPushBack (LPLLIST ll, LPVOID lpItem,
				LPLLNODE pnode);
extern LPLLNODE		IocpLLPushFront (LPLLIST ll, LPVOID lpItem,
				LPLLNODE pnode);
extern BOOL		IocpLLPop (LPLLNODE node, DWORD dwState);
extern BOOL		IocpLLPopAll (LPLLIST ll, LPLLNODE snode, DWORD dwState);
extern BOOL		IocpLLPopAllCompare (LPLLIST ll, LPVOID lpItem, DWORD dwState);
extern LPVOID		IocpLLPopBack (LPLLIST ll, DWORD dwState, DWORD timeout);
extern LPVOID		IocpLLPopFront (LPLLIST ll, DWORD dwState, DWORD timeout);
extern BOOL		IocpLLIsNotEmpty (LPLLIST ll);
extern BOOL		IocpLLNodeDestroy (LPLLNODE node);
extern SIZE_T		IocpLLGetCount (LPLLIST ll);

/* special hack jobs! */
extern BOOL PASCAL	OurConnectEx(SOCKET s, const struct sockaddr* name,
			    int namelen, PVOID lpSendBuffer, DWORD dwSendDataLength,
			    LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped);

/* some stuff that needs to be switches or fconfigures, but aren't yet */
#define IOCP_ACCEPT_COUNT	20
#define IOCP_ACCEPT_BUFSIZE	0    /* more than zero means we want a receive with the accept */
#define IOCP_RECV_COUNT		2
#define IOCP_RECV_BUFSIZE	2016  /* 2048 - 32 */
#define IOCP_SEND_CONCURRENCY	2
