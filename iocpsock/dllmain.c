#include "iocpsock.h"

/* A mess of stuff to make sure we get a good binary. */
#ifdef _MSC_VER
    // Only do this when MSVC++ is compiling us.
#   ifdef USE_TCL_STUBS
	// Mark this .obj as needing tcl's Stubs library.
#	pragma comment(lib, "tclstub" \
		STRINGIFY(JOIN(TCL_MAJOR_VERSION,TCL_MINOR_VERSION)) ".lib")
#	if !defined(_MT) || !defined(_DLL) || defined(_DEBUG)
	    // This fixes a bug with how the Stubs library was compiled.
	    // The requirement for msvcrt.lib from tclstubXX.lib should
	    // be removed.
#	    pragma comment(linker, "-nodefaultlib:msvcrt.lib")
#	endif
#   else
	// Mark this .obj needing the import library
#	pragma comment(lib, "tcl" \
		STRINGIFY(JOIN(TCL_MAJOR_VERSION,TCL_MINOR_VERSION)) ".lib")
#   endif
#   pragma comment (lib, "user32.lib")
#   pragma comment (lib, "kernel32.lib")
#endif


BOOL APIENTRY 
DllMain (HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{   
    if (dwReason == DLL_PROCESS_ATTACH) {
	/* don't call DLL_THREAD_ATTACH; I don't care to know. */
	DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

char *
GetSysMsg(DWORD id)
{
    int chars;
    static char sysMsgSpace[512];

    chars = wsprintf(sysMsgSpace, "[%u] ", id);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_IGNORE_INSERTS |
	    FORMAT_MESSAGE_MAX_WIDTH_MASK, 0L, id, 0, sysMsgSpace+chars, (512-chars),
	    0);
    return sysMsgSpace;
}

Tcl_Obj *
GetSysMsgObj(DWORD id)
{
    return Tcl_NewStringObj(GetSysMsg(id), -1);
}


/* Set the EXTERN macro to export declared functions. */
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT


/* This is the entry made to the dll (or static library) from Tcl. */
EXTERN int
Iocpsock_Init (Tcl_Interp *interp)
{
    Tcl_Obj *result;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
	return TCL_ERROR;
    }
#endif

    Tcl_MutexLock(&initLock);

    if (HasSockets(interp) == TCL_ERROR) {
	Tcl_MutexUnlock(&initLock);
	return TCL_ERROR;
    }
    result = Tcl_GetObjResult(interp);
    if (LOBYTE(winSock.wVersionLoaded) != 2) {
	Tcl_AppendObjToObj(result,
		Tcl_NewStringObj("Bad Winsock interface.  Need 2.x, but got ", -1));
	Tcl_AppendObjToObj(result, Tcl_NewDoubleObj(LOBYTE(winSock.wVersionLoaded)
		+ ((double)HIBYTE(winSock.wVersionLoaded)/10)));
	Tcl_AppendResult(interp, " instead.", NULL);
	winSock.WSACleanup();
	FreeLibrary(winSock.hModule);
	winSock.hModule = NULL;
	initialized = 0;
	Tcl_MutexUnlock(&initLock);
	return TCL_ERROR;
    }

    Tcl_MutexUnlock(&initLock);

    Tcl_CreateObjCommand(interp, "socket2", Iocp_SocketObjCmd, 0L, 0L);
    Tcl_PkgProvide(interp, "Iocpsock", "0.1");
    return TCL_OK;
}
