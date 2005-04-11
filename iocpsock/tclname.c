#include "tcl.h"
#include <errno.h>

#ifdef __WIN32__
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include "tclWinError.h"
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   pragma comment (lib, "ws2_32.lib")
#   ifdef _DEBUG
#	pragma comment (lib, "tcl85g.lib")
#   else
#	pragma comment (lib, "tcl85.lib")
#   endif
#endif


/* types */
enum Command {NAME_QUERY, NAME_REGISTER, NAME_UNREGISTER};
enum NameSpace {NAME_IP, NAME_IP4, NAME_IP6, NAME_IRDA};

/* protos */
int Init (CONST char *appName);
void Finalize();
Tcl_FileProc StdinReadable;
void ParseNameProtocol (Tcl_Obj *line);
void SendStart(void);
void SendReady(void);
void SendPosixErrorData (int protocolCode, CONST char *msg, int errorCode);
#ifdef __WIN32__
void SendWinErrorData (int protocolCode, CONST char *msg, DWORD errorCode);
#endif
void DoNameWork(enum Verb verb, enum NameSpace nameSpace, Tcl_Obj *arg1,
	       Tcl_Obj *arg2);

/* file scope globals */
int done = 0;
Tcl_Channel errChan, outChan, inChan;
Tcl_Obj *isIpRE_IPv4, *isIpRE_IPv6, *isIpRE_IPv6Comp, *isIpRE_4in6, *isIpRE_4in6Comp;


int
main (int argc, char *argv[])
{
    if (Init(argv[0]) == TCL_ERROR) {
	return EXIT_FAILURE;
    }
    while (!done) {
	Tcl_DoOneEvent(TCL_ALL_EVENTS);
    }
    Finalize();
    return EXIT_SUCCESS;
}

int
Init (CONST char *appName)
{
#ifdef __WIN32__
    WSADATA wsd;
    int err;
#endif

    Tcl_FindExecutable(appName);
    errChan = Tcl_GetStdChannel(TCL_STDERR);
    outChan = Tcl_GetStdChannel(TCL_STDOUT);
    inChan = Tcl_GetStdChannel(TCL_STDIN);

    Tcl_CreateChannelHandler(inChan, TCL_READABLE, StdinReadable, inChan);
    Tcl_SetChannelOption(NULL, inChan, "-buffering", "line");
    Tcl_SetChannelOption(NULL, inChan, "-blocking", "no");
    Tcl_SetChannelOption(NULL, inChan, "-encoding", "utf-8");

    isIpRE_IPv4 = Tcl_NewStringObj("^((25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)(\\.(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)){3})$", -1);
    Tcl_IncrRefCount(isIpRE_IPv4);
    isIpRE_IPv6 = Tcl_NewStringObj("^((?:[[:xdigit:]]{1,4}:){7}[[:xdigit:]]{1,4})$", -1);
    Tcl_IncrRefCount(isIpRE_IPv6);
    isIpRE_IPv6Comp = Tcl_NewStringObj("^((?:[[:xdigit:]]{1,4}(?::[[:xdigit:]]{1,4})*)?)::((?:[[:xdigit:]]{1,4}(?::[[:xdigit:]]{1,4})*)?)$", -1);
    Tcl_IncrRefCount(isIpRE_IPv6Comp);
    isIpRE_4in6 = Tcl_NewStringObj("^(((?:[[:xdigit:]]{1,4}:){6,6})(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)(\\.(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)){3})$", -1);
    Tcl_IncrRefCount(isIpRE_4in6);
    isIpRE_4in6Comp = Tcl_NewStringObj("^(((?:[[:xdigit:]]{1,4}(?::[[:xdigit:]]{1,4})*)?)::((?:[[:xdigit:]]{1,4}:)*)(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)(\\.(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)){3})$", -1);
    Tcl_IncrRefCount(isIpRE_4in6Comp);

#ifdef __WIN32__
    if ((err = WSAStartup(MAKEWORD(2,2), &wsd)) != 0) {
	SendWinErrorData(500, "can't start winsock with WSAStartup()", err);
        return TCL_ERROR;
    }
#endif
    SendStart();
    SendReady();
    return TCL_OK;
}

void
Finalize()
{
    Tcl_DecrRefCount(isIpRE_IPv4);
    Tcl_DecrRefCount(isIpRE_IPv6);
    Tcl_DecrRefCount(isIpRE_IPv6Comp);
    Tcl_DecrRefCount(isIpRE_4in6);
    Tcl_DecrRefCount(isIpRE_4in6Comp);
    Tcl_Finalize();
#ifdef __WIN32__
    WSACleanup();
#endif
}

void
StdinReadable (ClientData clientData, int mask)
{
    Tcl_Channel inChan = (Tcl_Channel) clientData;
    Tcl_Obj *line = Tcl_NewObj();
    int count;

    count = Tcl_GetsObj(inChan, line);
    if (count > 0) {
	ParseNameProtocol(line);
    } else if (count == -1) {
	if (Tcl_Eof(inChan) == 0) {
	    /* not EOF, get the error code */
	    SendPosixErrorData(500, "Tcl_GetsObj() failed", Tcl_GetErrno());
	}
	/* cause the event loop to drop-out, thus exit */
	done = 1;
    }
    Tcl_DecrRefCount(line);
}

void
ParseNameProtocol (Tcl_Obj *line)
{
    int objc;
    Tcl_Obj **objv;
    char *cmdStings[] = {
	"query", "register", "unregister", NULL
    };
    enum Command command;
    char *nsStings[] = {
	"ip", "ip4", "ip6", "irda", NULL
    };
    enum NameSpace nameSpace;
    Tcl_Obj *arg1, *arg2 = NULL;

    /* form is "<command> <namespace> <arg1> [<arg2>]" using Tcl list rules. */
    if (Tcl_ListObjGetElements(NULL, line, &objc, &objv) == TCL_OK) {
	if (objc < 3 || objc > 5) {
	    SendPosixErrorData(500, "improper number of arguments", EINVAL);
	    return;
	}
	/* get command */
	if (Tcl_GetIndexFromObj(NULL, objv[0], cmdStings, "", TCL_EXACT,
		&command) != TCL_OK) {
	    SendPosixErrorData(501, "no such command", EINVAL);
	    return;
	}
	/* get namespace */
	if (Tcl_GetIndexFromObj(NULL, objv[1], nsStings, "", TCL_EXACT,
		&nameSpace) != TCL_OK) {
	    SendPosixErrorData(502, "no such namespace", EINVAL);
	    return;
	}
	/* get arg1 */
	arg1 = objv[2];
	/* get arg2, if any. */
	if (objc > 3) {
	    arg2 = objv[3];
	}
	DoNameWork(command, nameSpace, arg1, arg2);
	SendReady();
    }
}

void
SendStart(void)
{
    Tcl_Obj *output = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("Tcl name resolver version: 1.0", -1));
    Tcl_WriteObj(outChan, output);
    Tcl_DecrRefCount(output);
    Tcl_WriteChars(outChan, "\n", -1);
}

void
SendReady(void)
{
    Tcl_Obj *output = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(200));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("ready", -1));
    Tcl_WriteObj(outChan, output);
    Tcl_DecrRefCount(output);
    Tcl_WriteChars(outChan, "\n", -1);
}

void
SendProtocolError (int protocolCode, CONST char *msg)
{
}

void
SendPosixErrorData (int protocolCode, CONST char *msg, int errorCode)
{
    Tcl_Obj *output = Tcl_NewObj();

    Tcl_SetErrno(errorCode);
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(protocolCode));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(msg, -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("POSIX", -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(Tcl_ErrnoId(), -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(errorCode));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(Tcl_ErrnoMsg(errorCode), -1));
    Tcl_WriteObj(outChan, output);
    Tcl_DecrRefCount(output);
    Tcl_WriteChars(outChan, "\n", -1);
}

#ifdef __WIN32__
void
SendWinErrorData (int protocolCode, CONST char *msg, DWORD errorCode)
{
    Tcl_Obj *output = Tcl_NewObj();

    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(protocolCode));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(msg, -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj("WIN32", -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(Tcl_Win32ErrId(errorCode), -1));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewIntObj(errorCode));
    Tcl_ListObjAppendElement(NULL, output, Tcl_NewStringObj(Tcl_Win32ErrMsg(errorCode, NULL), -1));
    Tcl_WriteObj(outChan, output);
    Tcl_DecrRefCount(output);
    Tcl_WriteChars(outChan, "\n", -1);
}
#endif

/***********************************************************************/

void Do_IP_Work(int addressFamily, Tcl_Obj *question);
void Do_IrDA_Work(enum Command command, Tcl_Obj *arg1, Tcl_Obj *arg2);
int isIp (Tcl_Obj *name);

void
DoNameWork (enum Command command, enum NameSpace nameSpace,
    Tcl_Obj *arg1, Tcl_Obj *arg2)
{
    int aFamily = 0;

    switch (nameSpace) {
	case NAME_IP:
	case NAME_IP4:
	case NAME_IP6:
	    switch (nameSpace) {
		case NAME_IP:
		    aFamily = AF_UNSPEC; break;
		case NAME_IP4:
		    aFamily = AF_INET; break;
		case NAME_IP6:
		    aFamily = AF_INET6; break;
	    }
	    if (command != NAME_QUERY) {
		SendPosixErrorData(504, "The IP namespace only supports the query command", EINVAL);
		return;
	    }
	    Do_IP_Work(aFamily, arg1);
	    break;
	case NAME_IRDA:
	    Do_IrDA_Work(command, arg1, arg2);
	    break;
    }
}

void
Do_IP_Work (int addressFamily, Tcl_Obj *question)
{
    struct addrinfo hints;
    struct addrinfo *hostaddr, *addr;
    int result, type;
    Tcl_Obj *answers, *output;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags  = 0;
    hints.ai_family = addressFamily;
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;

    result = getaddrinfo(Tcl_GetString(question), "", &hints, &hostaddr);

    if (result != 0) {
#ifdef __WIN32__
	SendWinErrorData(401, "lookup failed on getaddrinfo()", WSAGetLastError());
#else
	/* TODO */
#endif
	return;
    }

    answers = Tcl_NewObj();

    if (isIp(question)) {
	/* question was a numeric IP, return a hostname. */
	type = 0;
    } else {
	/* question was a hostname, return a numeric IP. */
	type = NI_NUMERICHOST;
    }

    addr = hostaddr;
    while (addr != NULL) {
	char hostStr[NI_MAXHOST];
	int err;

	err = getnameinfo(addr->ai_addr, addr->ai_addrlen, hostStr, NI_MAXHOST, NULL,
		0, type);
	if (err == 0) {
	    Tcl_ListObjAppendElement(NULL, answers, Tcl_NewStringObj(hostStr,-1));
	} else {
#ifdef __WIN32__
	    SendWinErrorData(401, "lookup failed on getnameinfo()", WSAGetLastError());
#else
	    /* TODO */
#endif
	    goto error;
	}

	addr = addr->ai_next;
    }

    /* reply code numeric */
    output = Tcl_NewStringObj("201", -1);
    Tcl_ListObjAppendElement(NULL, output, question);
    Tcl_ListObjAppendElement(NULL, output, answers);
    Tcl_WriteObj(outChan, output);
    Tcl_WriteChars(outChan, "\n", -1);
    Tcl_DecrRefCount(output);
error:
    freeaddrinfo(hostaddr);
    return;
}

int
isIp (Tcl_Obj *name)
{
    int a, b, c, d, e;
    a = Tcl_RegExpMatchObj(NULL, name, isIpRE_IPv4);
    b = Tcl_RegExpMatchObj(NULL, name, isIpRE_IPv6);
    c = Tcl_RegExpMatchObj(NULL, name, isIpRE_IPv6Comp);
    d = Tcl_RegExpMatchObj(NULL, name, isIpRE_4in6);
    e = Tcl_RegExpMatchObj(NULL, name, isIpRE_4in6Comp);
    if (a || b || c || d || e) {
	return 1;
    }
    return 0;
}

void
Do_IrDA_Work (enum Command command, Tcl_Obj *arg1, Tcl_Obj *arg2)
{
}
