/* ----------------------------------------------------------------------
 *
 * HttpdRead.c --
 *
 *	Performance enhanced replacement for the script procedure
 *	'HttpdRead' found in tclhttpd's lib/httpd.tcl.
 *
 * ----------------------------------------------------------------------
 * RCS: @(#) $Id$
 * ----------------------------------------------------------------------
 */

#include "iocpsock.h"

static int
IocpFastObjInvoke (
    Tcl_Interp *interp,		    /* Current interpreter. */
    unsigned int objc,		    /* Number of arguments. */
    struct Tcl_Obj * const objv[])  /* Argument objects. */
{
    const char **argv;
    unsigned int i;
    int result;
    Tcl_CmdInfo info;
    const char *cmd;

    cmd = Tcl_GetString(objv[0]);
    if (!Tcl_GetCommandInfo(interp, cmd, &info)) {
	Tcl_Obj *resultObj = Tcl_GetObjResult(interp);
	Tcl_AppendStringsToObj(resultObj, "Unknown command \"", cmd, "\"", NULL);
	return TCL_ERROR;
    }
    
    Tcl_ResetResult(interp);

    if (info.isNativeObjectProc) {
	for (i = 0; i < objc; i++) {
	    Tcl_IncrRefCount(objv[i]);
	}
	result = (*info.objProc)(info.objClientData, interp, objc, objv);
	for (i = 0; i < objc; i++) {
	    Tcl_DecrRefCount(objv[i]);
	}
    } else {
	argv = (const char **) ckalloc(objc * sizeof(const char *));
	for (i = 0; i < objc; i++) {
	    Tcl_IncrRefCount(objv[i]);
	    argv[i] = Tcl_GetString(objv[i]);
	}
	result = (*info.proc)(info.clientData, interp, objc, argv);
	for (i = 0; i < objc; i++) {
	    Tcl_DecrRefCount(objv[i]);
	}
	ckfree((char *)argv);
	Tcl_GetStringResult(interp);
    }

    return result;
}


int
Iocp_HttpdReadObjCmd (
    ClientData clientData,		/* really httpdReadConstants. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[])		/* Argument objects. */
{
    Tcl_Channel chan;
    Tcl_Obj *linePtr = Tcl_NewObj();
    Tcl_Obj *invoke[10];
    Tcl_Obj *data;
    Tcl_Obj *stateObj;
    httpdReadConstants *constants = (httpdReadConstants *) clientData;
    int length;
    enum states {DATA_START, NODATA_START, DATA_MIME, NODATA_MIME, ANYDATA_ERROR};

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "sock");
	return TCL_ERROR;
    }

    if ((chan = Tcl_GetChannel(interp, Tcl_GetString(objv[1]), 0L)) == 0L) {
	return TCL_ERROR;
    }

    /* upvar #0 Httpd$sock data */
    data = Tcl_NewStringObj("Httpd", -1);
    Tcl_AppendObjToObj(data, objv[1]);

    /* catch {gets $sock line} readCount */
    length = Tcl_GetsObj(chan, linePtr);
    if (length < 0 && !Tcl_Eof(chan) && !Tcl_InputBlocked(chan)) {
	Tcl_DecrRefCount(linePtr);
	invoke[0] = Tcl_NewStringObj("Httpd_SockClose", -1);
	invoke[1] = Tcl_NewStringObj(Tcl_GetChannelName(chan), -1);
	invoke[2] = Tcl_NewIntObj(1);
	invoke[3] = Tcl_NewStringObj("read error: error reading \"", -1);
	Tcl_AppendStringsToObj(invoke[3], Tcl_GetChannelName(chan), "\": ",
		Tcl_PosixError(interp), 0L);
	/* Httpd_SockClose $sock 1 "read error: $readCount" */
	IocpFastObjInvoke(interp, 4, invoke);
	return TCL_OK;
    }

    /* set state [string compare $readCount 0],$data(state) */
    stateObj = Tcl_ObjGetVar2(interp, data, constants->state, TCL_GLOBAL_ONLY);
    if (strcmp(Tcl_GetString(stateObj), "start") == 0) {
	if (length == 0) {
	    state = NODATA_START;
	} else if (length > 0) {
	    state = DATA_START;
	} else {
	    state = ANYDATA_ERROR;
	}
    } else if (strcmp(Tcl_GetString(stateObj), "mime") == 0) {
	if (length == 0) {
	    state = NODATA_MIME;
	} else if (length > 0) {
	    state = DATA_MIME;
	} else {
	    state = ANYDATA_ERROR;
	}
    } else if () {
    }

    /* TODO: add lots of code here. */

    return TCL_OK;
}