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
 * ------------------------------------------
 * High performance linked-list management.
 * ------------------------------------------
 */

/* Linked-List function state bitmask. */
#define LL_NOLOCK           0x80000000
#define LL_NODESTROY        0x40000000



extern LPLLIST	    LLCreate(LPCSTR szcName, INT iSz);
extern BOOL	    LLDestroy(LPLLIST ll, DWORD dwState);
extern LPLLNODE	    LLPushBack(LPLLIST ll, LPVOID lpItem,
			       LPLLNODE *dnode, LPLLNODE pnode);
extern LPLLNODE	    LLPushFront(LPLLIST ll, LPVOID lpItem,
				LPLLNODE *dnode, LPLLNODE pnode);
extern BOOL	    LLPop(LPLLNODE node, DWORD dwState);
extern BOOL	    LLPopAll(LPLLIST ll, LPLLNODE snode, DWORD dwState);
extern LPVOID	    LLPopBack(LPLLIST ll, DWORD dwState);
extern LPVOID	    LLPopFront(LPLLIST ll, DWORD dwState);
extern BOOL	    LLNodeDestroy(LPLLNODE node);
