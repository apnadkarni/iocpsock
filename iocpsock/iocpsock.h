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


#define IOCPSOCK_MAJOR_VERSION   3
#define IOCPSOCK_MINOR_VERSION   0
#define IOCPSOCK_RELEASE_LEVEL   TCL_ALPHA_RELEASE
#define IOCPSOCK_RELEASE_SERIAL  3

#define IOCPSOCK_VERSION	"3.0"
#define IOCPSOCK_PATCH_LEVEL	"3.0a3"


#ifndef RC_INVOKED

#undef TCL_STORAGE_CLASS
#ifdef BUILD_iocp
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   ifdef USE_IOCP_STUBS
#	define TCL_STORAGE_CLASS
#   else
#	define TCL_STORAGE_CLASS DLLIMPORT
#   endif
#endif


/*
 * Fix the Borland bug that's in the EXTERN macro from tcl.h.
 */
#ifndef TCL_EXTERN
#   undef DLLIMPORT
#   undef DLLEXPORT
#   ifdef __cplusplus
#	define TCL_EXTERNC extern "C"
#   else
#	define TCL_EXTERNC extern
#   endif
#   if defined(STATIC_BUILD)
#	define DLLIMPORT
#	define DLLEXPORT
#	define TCL_EXTERN(RTYPE) TCL_EXTERNC RTYPE
#   elif (defined(__WIN32__) && ( \
	    defined(_MSC_VER) || (__BORLANDC__ >= 0x0550) || \
	    defined(__LCC__) || defined(__WATCOMC__) || \
	    (defined(__GNUC__) && defined(__declspec)) \
	)) || (defined(MAC_TCL) && FUNCTION_DECLSPEC)
#	define DLLIMPORT __declspec(dllimport)
#	define DLLEXPORT __declspec(dllexport)
#	define TCL_EXTERN(RTYPE) TCL_EXTERNC TCL_STORAGE_CLASS RTYPE
#   elif defined(__BORLANDC__)
#	define DLLIMPORT __import
#	define DLLEXPORT __export
	/* Pre-5.5 Borland requires the attributes be placed after the */
	/* return type instead. */
#	define TCL_EXTERN(RTYPE) TCL_EXTERNC RTYPE TCL_STORAGE_CLASS
#   else
#	define DLLIMPORT
#	define DLLEXPORT
#	define TCL_EXTERN(RTYPE) TCL_EXTERNC TCL_STORAGE_CLASS RTYPE
#   endif
#endif

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


#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif  /* #ifndef RC_INVOKED */
#endif /* #ifndef INCL_iocpsock_h_ */
