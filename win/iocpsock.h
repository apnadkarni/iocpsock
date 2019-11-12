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


#include "tcl.h"


#define IOCPSOCK_VERSION	PACKAGE_VERSION

#ifndef RC_INVOKED

#undef TCL_STORAGE_CLASS
#ifdef BUILD_Iocpsock
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   ifdef USE_IOCP_STUBS
#	define TCL_STORAGE_CLASS
#   else
#	define TCL_STORAGE_CLASS DLLIMPORT
#   endif
#endif

#ifdef __cplusplus
#	define TCL_EXTERNC extern "C"
#else
# define TCL_EXTERNC extern
#endif
#define TCL_EXTERN(RTYPE) EXTERN RTYPE

/*
 * Include the public function declarations that are accessible via
 * the stubs table.
 */

#include "iocpDecls.h"

#ifdef USE_IOCP_STUBS
    TCL_EXTERNC CONST char *
	Iocpsock_InitStubs _ANSI_ARGS_((Tcl_Interp *interp,
		CONST char *version, int exact));
#else
#   define Iocpsock_InitStubs(interp, version, exact) \
	Tcl_PkgRequire(interp, "Iocpsock", version, exact)
#endif

#endif  /* #ifndef RC_INVOKED */
#endif /* #ifndef INCL_iocpsock_h_ */
