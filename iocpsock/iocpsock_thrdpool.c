#include "iocpsockInt.h"

typedef void (Iocp_WaitCallbackProc) (ClientData handle,
	ClientData userData, int timedOut);

typedef struct IocpWaitCallbackInfo {
    Iocp_WaitCallbackProc *proc;   /* function to call when done
				     * (signaled or timedout) */
    ClientData userData;	    /* user data for the callback */
    ClientData handle;		    /* OS defined handle type */
} IocpWaitCallbackInfo;


Iocp_WaitCallbackProc IocpResolveWaitDoneProc;

VOID CALLBACK
WaitCallback (PVOID lpParameter, BOOLEAN timedOut)
{
    IocpWaitCallbackInfo *info = (IocpWaitCallbackInfo *) lpParameter;
    __try {
	info->proc(info->handle, info->userData,
		(timedOut == TRUE ? 1 : 0));
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void
IocpResolveWaitDoneProc (ClientData handle, ClientData userData,
	int timedOut)
{
}

/*
 *  Register for a single async callback when the "file" becomes ready
 *  or a timeout occurs.
 */

int
Iocp_RegisterAsyncWait (
    Tcl_Time *timeOutPtr,	     /* timeout value */
    Iocp_WaitCallbackProc callback,  /* func to call */
    ClientData handle,		     /* OS native handle 
				      * (for cancel or repost) */
    ClientData userData,	     /* data for callback */
    int flags)			     /* IOCP_ONCE or IOCP_FOREVER */
{
    HANDLE newWaitHandle;
    DWORD timeOutMSecs;
    HANDLE waitObject = (HANDLE) handle;
    IocpWaitCallbackInfo *info = NULL;
    BOOL ok;

    info = (IocpWaitCallbackInfo *) ckalloc(sizeof(IocpWaitCallbackInfo));
    info->proc = callback;
    info->userData = userData;

    if (timeOutPtr == NULL) {
	timeOutMSecs = INFINITE;
    } else {
	timeOutMSecs = (timeOutPtr->sec*1000)+(timeOutPtr->usec/1000);
    }

    /* Win2K/WinXP only.  TODO: need NT/ME/98 compat way, too */
    ok = RegisterWaitForSingleObject(&newWaitHandle,
	    waitObject, WaitCallback, info, timeOutMSecs,
	    WT_EXECUTEONLYONCE);

    if (!ok) {
	/* TODO: error stuff. */
    }
    return TCL_OK;
}