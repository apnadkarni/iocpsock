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

###  Some Win32 error stuff the core is missing.

declare 2 generic {
    CONST char *Tcl_WinErrId (unsigned int errorCode)
}
declare 3 generic {
    CONST char *Tcl_WinErrMsg (unsigned int errorCode,
	va_list *extra)
}
declare 4 generic {
    CONST char *Tcl_WinError (Tcl_Interp *interp,
	unsigned int errorCode, va_list *extra)
}

### TCP stuff

declare 5 generic {
    Tcl_Channel Iocp_OpenTcpClient (Tcl_Interp *interp,
	CONST char *port, CONST char *host, CONST char *myaddr,
	CONST char *myport, int async)
}
declare 6 generic {
    Tcl_Channel Iocp_OpenTcpServer (Tcl_Interp *interp,
	CONST char *port, CONST char *host,
	Tcl_TcpAcceptProc *acceptProc, ClientData acceptProcData)
}
declare 7 generic {
    Tcl_Channel Iocp_MakeTcpClientChannel (ClientData sock)
}

### UDP stuff

#declare 8 generic {
#    Tcl_Channel Iocp_OpenUdpClient (Tcl_Interp *interp,
#	CONST char *port, CONST char *host, CONST char *myaddr,
#	CONST char *myport, int async)
#}
#declare 9 generic {
#    Tcl_Channel Iocp_MakeUdpClientChannel (ClientData sock)
#}

### IrDA stuff
### IPX/SPX stuff


interface iocpInt

declare 0 generic {
    Tcl_Obj * DecodeIpSockaddr (SocketInfo *info, LPSOCKADDR addr)
}
declare 1 generic {
    Tcl_Obj * DecodeIrdaSockaddr (SocketInfo *info, LPSOCKADDR addr)
}
declare 2 generic {
    Tcl_Obj * DecodeIpxSockaddr (SocketInfo *info, LPSOCKADDR addr)
}
declare 3 generic {
    ThreadSpecificData *InitSockets(void)
}
declare 4 generic {
    void IocpInitProtocolData (SOCKET sock, WS2ProtocolData *pdata)
}
declare 5 generic {
    int CreateSocketAddress (const char *addr, const char *port,
	LPADDRINFO inhints, LPADDRINFO *result)
}
declare 6 generic {
    void FreeSocketAddress(LPADDRINFO addrinfo)
}
declare 7 generic {
    BOOL FindProtocolInfo(int af, int type, int protocol, DWORD flags,
	WSAPROTOCOL_INFO *pinfo)
}
declare 8 generic {
    DWORD PostOverlappedAccept (SocketInfo *infoPtr,
	BufferInfo *acceptobj, int useBurst)
}
declare 9 generic {
    DWORD PostOverlappedRecv (SocketInfo *infoPtr,
	BufferInfo *recvobj, int useBurst, int ForcePostOnError)
}
declare 10 generic {
    DWORD PostOverlappedQOS (SocketInfo *infoPtr, BufferInfo *bufPtr)
}
declare 11 generic {
    void IocpWinConvertWSAError(DWORD errCode)
}
declare 12 generic {
    void FreeBufferObj(BufferInfo *obj)
}
declare 13 generic {
    BufferInfo * GetBufferObj (SocketInfo *infoPtr, SIZE_T buflen)
}
declare 14 generic {
    SocketInfo * NewSocketInfo (SOCKET socket)
}
declare 15 generic {
    void FreeSocketInfo (SocketInfo *infoPtr)
}
declare 16 generic {
    int HasSockets (Tcl_Interp *interp)
}
#declare 17 generic {
#    char * GetSysMsg (DWORD id)
#}
#declare 18 generic {
#    Tcl_Obj * GetSysMsgObj (DWORD id)
#}
declare 19 generic {
    int Iocp_IrdaDiscovery (Tcl_Interp *interp, Tcl_Obj **deviceList,
	int limit)
}
declare 20 generic {
    int Iocp_IrdaIasQuery (Tcl_Interp *interp, Tcl_Obj *deviceId,
	Tcl_Obj *serviceName, Tcl_Obj *attribName, Tcl_Obj **value)
}
declare 21 generic {
    LPLLIST IocpLLCreate (void)
}
declare 22 generic {
    BOOL IocpLLDestroy (LPLLIST ll)
}
declare 23 generic {
    LPLLNODE IocpLLPushBack (LPLLIST ll, LPVOID lpItem, LPLLNODE pnode,
	DWORD dwState)
}
declare 24 generic {
    LPLLNODE IocpLLPushFront (LPLLIST ll, LPVOID lpItem, LPLLNODE pnode,
	DWORD dwState)
}
declare 25 generic {
    BOOL IocpLLPop (LPLLNODE pnode, DWORD dwState)
}
declare 26 generic {
    BOOL IocpLLPopAll (LPLLIST ll, LPLLNODE snode, DWORD dwState)
}
declare 27 generic {
    LPVOID IocpLLPopBack (LPLLIST ll, DWORD dwState, DWORD timeout)
}
declare 28 generic {
    LPVOID IocpLLPopFront (LPLLIST ll, DWORD dwState, DWORD timeout)
}
declare 29 generic {
    BOOL IocpLLIsNotEmpty (LPLLIST ll)
}
declare 30 generic {
    BOOL IocpLLNodeDestroy (LPLLNODE node)
}
declare 31 generic {
    SIZE_T IocpLLGetCount (LPLLIST ll)
}
