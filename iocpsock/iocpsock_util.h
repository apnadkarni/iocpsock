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



extern LPLLIST	    TSLLCreate();
extern BOOL	    TSLLDestroy(LPLLIST ll, DWORD dwState);
extern LPLLNODE	    TSLLPushBack(LPLLIST ll, LPVOID lpItem,
			       /*LPLLNODE *dnode,*/ LPLLNODE pnode);
extern LPLLNODE	    TSLLPushFront(LPLLIST ll, LPVOID lpItem,
				/*LPLLNODE *dnode,*/ LPLLNODE pnode);
extern BOOL	    TSLLPop(LPLLNODE node, DWORD dwState);
extern BOOL	    TSLLPopAll(LPLLIST ll, LPLLNODE snode, DWORD dwState);
extern LPVOID	    TSLLPopBack(LPLLIST ll, DWORD dwState);
extern LPVOID	    TSLLPopFront(LPLLIST ll, DWORD dwState);
extern BOOL	    TSLLIsNotEmpty(LPLLIST ll);
extern BOOL	    TSLLNodeDestroy(LPLLNODE node);
