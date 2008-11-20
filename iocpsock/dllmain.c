#include "iocpsockInt.h"

Tcl_ObjCmdProc	Iocp_StatsObjCmd;

/* Globals */
HMODULE iocpModule = NULL;

/* A mess of stuff to make sure we get a good binary. */
#ifdef _MSC_VER
    // Only do this when MSVC++ is compiling us.
#   if defined(USE_TCL_STUBS)
	// Mark this .obj as needing tcl's Stubs library.
#	pragma comment(lib, "tclstub" \
		STRINGIFY(JOIN(TCL_MAJOR_VERSION,TCL_MINOR_VERSION)) ".lib")
#	if !defined(_MT) || !defined(_DLL) || defined(_DEBUG)
	    // This fixes a bug with how the Stubs library was compiled.
	    // The requirement for msvcrt.lib from tclstubXX.lib should
	    // be removed.
#	    pragma comment(linker, "-nodefaultlib:msvcrt.lib")
#	endif
#   elif !defined(STATIC_BUILD)
	// Mark this .obj needing the import library
#	pragma comment(lib, "tcl" \
		STRINGIFY(JOIN(TCL_MAJOR_VERSION,TCL_MINOR_VERSION)) ".lib")
#   endif
#   pragma comment (lib, "user32.lib")
#   pragma comment (lib, "kernel32.lib")
#endif


#if !defined(STATIC_BUILD)
BOOL APIENTRY 
DllMain (HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{   
    if (dwReason == DLL_PROCESS_ATTACH) {
	/* don't call DLL_THREAD_ATTACH; I don't care to know. */
	DisableThreadLibraryCalls(hModule);
	iocpModule = hModule;
    }
    return TRUE;
}
#endif

static const char *HtmlStart = "<tr><td>";
static const char *HtmlMiddle = "</td><td>";
static const char *HtmlEnd = "</td></tr>\n";

int
Iocp_StatsObjCmd (
    ClientData notUsed,			/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[])		/* Argument objects. */
{
    Tcl_Obj *newResult;
    char buf[TCL_INTEGER_SPACE];
    int useHtml = 0;
    const char *Start = "", *Middle = " ", *End = "\n";

    if (objc == 2) {
	if (Tcl_GetBooleanFromObj(interp, objv[1], &useHtml) == TCL_ERROR) {
	    return TCL_ERROR;
	}
    }

    if (useHtml) {
	Start = HtmlStart;
	Middle = HtmlMiddle;
	End = HtmlEnd;
    }

    newResult = Tcl_NewObj();
    Tcl_AppendStringsToObj(newResult, Start, "Sockets open:", Middle, 0L);
    sprintf(buf, "%ld", StatOpenSockets);
    Tcl_AppendStringsToObj(newResult, buf, End, Start, "AcceptEx calls that returned an error:", Middle, 0L);
    sprintf(buf, "%ld", StatFailedAcceptExCalls);
    Tcl_AppendStringsToObj(newResult, buf, End, Start, "Unreplaced AcceptEx calls:", Middle, 0L);
    sprintf(buf, "%ld", StatFailedReplacementAcceptExCalls);
    Tcl_AppendStringsToObj(newResult, buf, End, Start, "General pool bytes in use:", Middle, 0L);
    sprintf(buf, "%ld", StatGeneralBytesInUse);
    Tcl_AppendStringsToObj(newResult, buf, End, Start, "Special pool bytes in use:", Middle, 0L);
    sprintf(buf, "%ld", StatSpecialBytesInUse);
    Tcl_AppendStringsToObj(newResult, buf, End, 0L);
    Tcl_SetObjResult(interp, newResult);
    return TCL_OK;
}


/* This is the entry made to the dll (or static library) from Tcl. */
int
Iocpsock_Init (Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.3", 0) == NULL) {
	return TCL_ERROR;
    }
#endif

    Tcl_MutexLock(&initLock);
    if (HasSockets(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_MutexUnlock(&initLock);

    Tcl_CreateObjCommand(interp, "socket2", Iocp_SocketObjCmd, 0L, 0L);
    Tcl_CreateObjCommand(interp, "iocp_stats", Iocp_StatsObjCmd, 0L, 0L);
//    Tcl_CreateObjCommand(interp, "irda::discovery", Iocp_IrdaDiscoveryCmd, 0L, 0L);
//    Tcl_CreateObjCommand(interp, "irda::ias_query", Iocp_IrdaIasQueryCmd, 0L, 0L);
//    Tcl_CreateObjCommand(interp, "irda::ias_set", Iocp_IrdaIasSetCmd, 0L, 0L);
//    Tcl_CreateObjCommand(interp, "irda::lazy_discovery", Iocp_IrdaLazyDiscoveryCmd, 0L, 0L);
    Tcl_PkgProvide(interp, "Iocpsock", IOCPSOCK_VERSION);
    return TCL_OK;
}

int
Iocpsock_SafeInit (Tcl_Interp *interp)
{
    return Iocpsock_Init(interp);
}
