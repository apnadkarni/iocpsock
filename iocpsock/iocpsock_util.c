#include "iocpsock.h"
#include "iocpsock_util.h"


/* Bitmask macros. */
#define mask_a( mask, val ) if ( ( mask & val ) != val ) { mask |= val; }
#define mask_d( mask, val ) if ( ( mask & val ) == val ) { mask ^= val; }
#define mask_y( mask, val ) ( mask & val ) == val
#define mask_n( mask, val ) ( mask & val ) != val


/* Creates a linked list. */
LPLLIST
TSLLCreate ()
{   
    LPLLIST ll;
    
    /* Alloc a linked list. */
    ll = (LPLLIST) HeapAlloc(IocpSubSystem.heap, HEAP_ZERO_MEMORY,
	    sizeof(LLIST));
    if (!ll) {
	return NULL;
    }
    if (!InitializeCriticalSectionAndSpinCount(&ll->lock, 4000)) {
	HeapFree(IocpSubSystem.heap, 0, ll);
	return NULL;
    }
    ll->lCount = 0;
    return ll;
}

//Destroyes a linked list.
BOOL 
TSLLDestroy(
    LPLLIST ll,
    DWORD dwState)
{   
    //Destroy the linked list.
    if (mask_n(dwState, LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (ll->lCount) {
//	cout << "\nLinked List Memory Leak: " << ll->lCount << " items\n";
    }
    if (mask_n(dwState, LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    DeleteCriticalSection(&ll->lock);
    return HeapFree(IocpSubSystem.heap, 0, ll);
}

//Adds an item to the end of the list.
LPLLNODE 
TSLLPushBack(
    LPLLIST ll,
    LPVOID lpItem,
//    LPLLNODE *dnode,
    LPLLNODE pnode)
{
//    BOOL alloc;
    LPLLNODE tmp;

    EnterCriticalSection(&ll->lock);
//    alloc = FALSE;
    if (!pnode) {
	pnode = (LPLLNODE) HeapAlloc(IocpSubSystem.heap, HEAP_ZERO_MEMORY, sizeof(LLNODE));
//	alloc = TRUE;
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
//    pnode->lppNode = dnode; 
    LeaveCriticalSection(&ll->lock);
    return pnode;
}

//Adds an item to the front of the list.
LPLLNODE 
TSLLPushFront(
    LPLLIST ll,
    LPVOID lpItem,
//    LPLLNODE *dnode, 
    LPLLNODE pnode)
{
    BOOL alloc;
    LPLLNODE tmp;

    EnterCriticalSection(&ll->lock);
    alloc = FALSE;
    if (!pnode) {
	pnode = (LPLLNODE) HeapAlloc(IocpSubSystem.heap, HEAP_ZERO_MEMORY, sizeof(LLNODE));
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
//    pnode->lppNode = dnode; 
    LeaveCriticalSection(&ll->lock);
    return pnode;
}

//Removes all items from the list.
BOOL 
TSLLPopAll(
    LPLLIST ll,
    LPLLNODE snode,
    DWORD dwState)
{
    LPLLNODE tmp1, tmp2;

    if (snode && snode->ll) {
	ll = snode->ll;
    }
    if (mask_n(dwState, LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && ! ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, LL_NOLOCK)) {
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
        if (mask_n(dwState, LL_NODESTROY)) {
//	    if (tmp1->lppNode) {
//		*tmp1->lppNode = NULL;
//	    }
            HeapFree(IocpSubSystem.heap, 0, tmp1);
	} else {
	    tmp1->ll = NULL;
	    tmp1->next = NULL; 
            tmp1->prev = NULL;
	}
        ll->lCount--;
	tmp1 = tmp2;
    }

    if (mask_n(dwState, LL_NOLOCK)) {
	LeaveCriticalSection( &ll->lock );
    }
    
    return TRUE;
}

//Removes an item from the list.
BOOL 
TSLLPop(
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
    if (mask_n(dwState, LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && !ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, LL_NOLOCK)) {
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
    if (mask_n(dwState, LL_NODESTROY)) {
	TSLLNodeDestroy( node );
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

    if (mask_n(dwState, LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    return TRUE;
}

//Destroys a node.
BOOL
TSLLNodeDestroy(
    LPLLNODE node)
{
//    LPLLNODE *lppNode = node->lppNode;
    BOOL ret = HeapFree(IocpSubSystem.heap, 0, node);
//    if (lppNode) {
//	*lppNode = NULL;
//    }
    return ret;
}

//Removes the item at the back of the list.
LPVOID
TSLLPopBack(
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
    TSLLPop(tmp, LL_NOLOCK | dwState);
    LeaveCriticalSection(&ll->lock);
    return vp;
}

//Removes the item at the front of the list.
LPVOID
TSLLPopFront(
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
    TSLLPop(tmp, LL_NOLOCK | dwState);
    LeaveCriticalSection(&ll->lock);
    return vp;
}

BOOL
TSLLIsNotEmpty(LPLLIST ll)
{
    return (ll->lCount != 0 ? TRUE : FALSE);
}