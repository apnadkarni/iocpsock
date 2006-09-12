/* 
 * iocpStubLib.c --
 *
 *	Stub object that will be statically linked into extensions that wish
 *	to access Expect.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: $Id$
 */

/*
 * We need to ensure that we use the stub macros so that this file contains
 * no references to any of the stub functions.  This will make it possible
 * to build an extension that references Tcl_InitStubs but doesn't end up
 * including the rest of the stub functions.
 */

#ifndef USE_TCL_STUBS
#define USE_TCL_STUBS
#endif
#undef USE_TCL_STUB_PROCS

/*
 * This ensures that the Iocpsock_InitStubs has a prototype in
 * iocpsock.h and is not the macro that turns it into Tcl_PkgRequire
 */

#ifndef USE_IOCP_STUBS
#define USE_IOCP_STUBS
#endif

#include "iocpsockInt.h"

IocpStubs	*iocpStubsPtr;
IocpIntStubs	*iocpIntStubsPtr;

/*
 *----------------------------------------------------------------------
 *
 * Iocpsock_InitStubs --
 *
 *	Tries to initialise the stub table pointers and ensures that
 *	the correct version of Iocpsock is loaded.
 *
 * Results:
 *	The actual version of Iocpsock that satisfies the request, or
 *	NULL to indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */

CONST char *
Iocpsock_InitStubs (interp, version, exact)
    Tcl_Interp *interp;
    CONST char *version;
    int exact;
{
    CONST char *actualVersion;
    
    actualVersion = Tcl_PkgRequireEx(interp, "Iocpsock", version, exact,
        (ClientData *) &iocpStubsPtr);

    if (actualVersion == NULL) {
	iocpStubsPtr = NULL;
	return NULL;
    }

    if (iocpStubsPtr->hooks) {
	iocpIntStubsPtr = iocpStubsPtr->hooks->iocpIntStubs;
    } else {
	iocpIntStubsPtr = NULL;
    }
    
    return actualVersion;
}
