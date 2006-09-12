# iocpsock.decls --
#
#	This file contains the declarations for all supported public
#	functions that are exported by the iocpsock library via the stubs table.
#	This file is used to generate the itclDecls.h, itclPlatDecls.h,
#	itclStub.c, and itclPlatStub.c files.
#	
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: $Id$

library iocp

# Define the itcl interface with several sub interfaces:
#     itclPlat	 - platform specific public
#     itclInt	 - generic private
#     itclPlatInt - platform specific private

interface iocp
hooks {iocpInt}

# Declare each of the functions in the public Tcl interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

declare 0 generic {
    int Iocpsock_Init (Tcl_Interp *interp)
}
declare 1 generic {
    int Iocpsock_SafeInit (Tcl_Interp *interp)
}
declare 2 generic {
    Tcl_Channel Iocp_OpenTcpClient (Tcl_Interp *interp,
	CONST char *port, CONST char *host, CONST char *myaddr,
	CONST char *myport, int async)
}
declare 3 generic {
    Tcl_Channel Iocp_OpenTcpServer (Tcl_Interp *interp,
	CONST char *port, CONST char *host,
	Tcl_TcpAcceptProc *acceptProc, ClientData acceptProcData)
}
declare 4 generic {
    Tcl_Channel Iocp_OpenUdpSocket (Tcl_Interp *interp,
	CONST char *port, CONST char *host, CONST char *myaddr,
	CONST char *myport)
}
declare 5 generic {
    Tcl_Channel Iocp_OpenIrdaClient (Tcl_Interp *interp,
	CONST char *ServiceName, CONST char *DeviceId,
	CONST char *myDeviceId, CONST char *myServiceName,
	int async)
}
declare 6 generic {
    Tcl_Channel Iocp_OpenIrdaServer (Tcl_Interp *interp,
	CONST char *serviceName, CONST char *DeviceId,
	CONST char *myDeviceId, CONST char *myServiceName,
	int async)
}

interface iocpInt