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

/* protocol specific info (standard LSPs) */
//#include <af_irda.h>
//#include <ws2atm.h>
//#include <ws2dnet.h>
//#include <wshisotp.h>
//#include <wsipx.h>
//#include <wsnetbs.h>
//#include <wsnwlink.h>
//#include <wsvns.h>

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

/*
 * Only one subsystem is ever created, but we group it within a
 * struct just to be organized.
 */
typedef struct {
    HANDLE port;	    /* The completion port handle. */
    HANDLE heap;	    /* Private heap used for WSABUFs and other
			     * objects that don't need to interact with
			     * Tcl directly. */
#   define MAX_COMPLETION_THREAD_COUNT	32
			    /* Maximum number of completion threads
			     * allowed. */
    HANDLE threads[MAX_COMPLETION_THREAD_COUNT];
			    /* The array of threads for handling the
			     * completion routines. */
} CompletionPortInfo;
extern CompletionPortInfo IocpSubSystem;

/*
 * Specific protocol information is stored here and shared to all
 * SocketInfo objects of that type.
 */
typedef struct {
    int af;		    /* Address family. */
    int	type;		    /* Address type. */
    int	protocol;		    /* protocol type specific to the address
			     * family. */
    LPFN_ACCEPTEX AcceptEx; /* LSP specific function for AcceptEx(). */
    LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrs;
			    /* LSP specific function for
			     * GetAcceptExSockaddrs(). */
} WS2ProtocolData;

extern GUID gAcceptExGuid;  /* GUID used for the WSAIoctl call to get
			     * AcceptEx() from the LSP. */
extern GUID gGetAcceptExSockaddrsGuid;
			    /* GUID used for the WSAIoctl call to get
			     * GetAcceptExSockaddrs() from the LSP. */

/*
 * This is our per I/O buffer. It contains a WSAOVERLAPPED structure as well
 * as other necessary information for handling an IO operation on a socket.
 */
typedef struct _BUFFER_OBJ {
    WSAOVERLAPPED ol;
    SOCKET socket;	    // Used for AcceptEx client socket
    BYTE *buf;		    // Buffer for recv/send/AcceptEx
    size_t buflen;	    // Length of the buffer
#   define OP_ACCEPT	0   // AcceptEx
#   define OP_READ	1   // WSARecv/WSARecvFrom
#   define OP_WRITE	2   // WSASend/WSASendTo
    int operation;	    // Type of operation issued
    SOCKADDR_STORAGE addr;  // addr storage space.
    int addrlen;	    // length of the protocol specific address
    ULONG IoOrder;	    // Order in which this I/O was posted
    struct _BUFFER_OBJ  *next;
} BUFFER_OBJ;


#include "tclInt.h"

// We need at least the Tcl_Obj interface that was started in 8.0
#if TCL_MAJOR_VERSION < 8
#   error "we need Tcl 8.0 or greater to build this"

// Check for Stubs compatibility when asked for it.
#elif defined(USE_TCL_STUBS) && TCL_MAJOR_VERSION == 8 && \
	(TCL_MINOR_VERSION == 0 || \
        (TCL_MINOR_VERSION == 1 && TCL_RELEASE_LEVEL != TCL_FINAL_RELEASE))
#   error "Stubs interface doesn't work in 8.0 and alpha/beta 8.1; only 8.1.0+"
#endif


typedef struct ThreadSpecificData {
    Tcl_AsyncHandler asyncToken;
    Tcl_ThreadId     threadId;
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;


/* Linked-List node object. */
struct S_ListNode;
struct S_List;
typedef struct S_ListNode
{
    struct S_ListNode *next;
    struct S_ListNode *prev;
    struct S_List *ll;
    struct S_ListNode **lppNode;
    LPVOID lpItem;
}   LLNODE, *LPLLNODE;

/* Linked-List object. */
typedef struct S_List
{
    struct S_ListNode *front;
    struct S_ListNode *back;
    LPSTR szName;
    INT iNameSz;
    LONG lCount;
    CRITICAL_SECTION lock;
}   LLIST, *LPLLIST;

/*
 * The following structure is used to store the data associated with
 * each socket.
 */

typedef struct SocketInfo {
    Tcl_Channel channel;	   /* Channel associated with this
				    * socket. */
    SOCKET socket;		   /* Windows SOCKET handle. */
    WS2ProtocolData *proto;
    CRITICAL_SECTION critSec;	    /* accessor lock */
    Tcl_AsyncHandler asyncToken;    /* What notifier to alert when I/O is ready */
    Tcl_ThreadId threadId;	    /* parent thread */
    int readyMask;		    /* Are we ready for a read or write? */
    BOOL bClosing;
    Tcl_TcpAcceptProc *acceptProc; /* Proc to call on accept. */
    ClientData acceptProcData;	   /* The data for the accept proc. */
    DWORD lastError;		   /* Error code from last message. */
    volatile LONG OutstandingOps;	    
    ULONG LastSendIssued; // Last sequence number sent
    ULONG IoCountIssued;
    BUFFER_OBJ *OutOfOrderSends;// List of send buffers that completed out of order
    BUFFER_OBJ **PendingAccepts;    // Pending AcceptEx buffers 
				    //   (used for listening sockets only)
    LPLLIST llPendingRecv; //Our pending recv list.
    LPLLIST llPendingSend; //Our pending send list.

} SocketInfo;


extern Tcl_ChannelType IocpChannelType;


TCL_DECLARE_MUTEX(initLock)

extern int		CreateSocketAddress (CONST char *addr,
				CONST char *port, WS2ProtocolData *pdata,
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
extern DWORD		PostOverlappedAccept (SocketInfo *si,
				BUFFER_OBJ *acceptobj);
extern BUFFER_OBJ *	GetBufferObj (SocketInfo *si, size_t buflen);
extern SocketInfo *	NewSocketInfo(SOCKET socket);
extern int		HasSockets(Tcl_Interp *interp);
extern char *		GetSysMsg(DWORD id);
extern Tcl_Obj *	GetSysMsgObj(DWORD id);
extern Tcl_ObjCmdProc	Iocp_SocketObjCmd;
