#include "iocpsockInt.h"
#include <atalkwsh.h>

static FN_DECODEADDR DecodeAplTlkSockaddr;

static WS2ProtocolData apltalkProtoData = {
    AF_APPLETALK,
    SOCK_STREAM,
    ATPROTO_ATP,
    sizeof(SOCKADDR_AT),
    DecodeAplTlkSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static Tcl_Obj *
DecodeAplTlkSockaddr (SocketInfo *info, LPSOCKADDR addr)
{
    return Tcl_NewObj();
}


#if 0
 // ??????????????
//
// copyright (c) Taisoft
// ???????: cl adsp.c wsock32.lib
//
// adsp.c
#include "stdio.h"
#include "windows.h"
#include "winsock.h"
#include <atalkwsh.h>

char *gServerName="=";
char *gServerType="=";

int LookupServer(int s,char *zone)
{
    int    r;
    char buf[1024*12];
    PWSH_LOOKUP_NAME pLookup;
    WSH_NBP_TUPLE    *tuple;
    DWORD    wsiz;
    int    i,j;
    char n[128];
    char *sv,*sv2;

    pLookup = (PWSH_LOOKUP_NAME)buf;

    strcpy(pLookup->LookupTuple.NbpName.ObjectName, gServerName);
    pLookup->LookupTuple.NbpName.ObjectNameLen = strlen(gServerName);
    strcpy(pLookup->LookupTuple.NbpName.TypeName, gServerType);
    pLookup->LookupTuple.NbpName.TypeNameLen = strlen(gServerType);
    strcpy(pLookup->LookupTuple.NbpName.ZoneName, zone);
    pLookup->LookupTuple.NbpName.ZoneNameLen = strlen(zone);

    wsiz = sizeof(buf);
    r = getsockopt(s, SOL_APPLETALK, SO_LOOKUP_NAME,(char*)buf,&wsiz);

    if(r != NO_ERROR){
        printf("getsockopt:error = %d\n", WSAGetLastError());
        return -1;
    }
    i=((PWSH_LOOKUP_NAME)buf)->NoTuples;
    tuple=(PWSH_NBP_TUPLE)(buf+sizeof(WSH_LOOKUP_NAME));
    for(;i>0;i--,tuple++){
        memcpy(n,tuple->NbpName.ObjectName,tuple->NbpName.ObjectNameLen);
        n[tuple->NbpName.ObjectNameLen]=0;
        printf("<%s>:",n);
        memcpy(n,tuple->NbpName.TypeName,tuple->NbpName.TypeNameLen);
        n[tuple->NbpName.TypeNameLen]=0;
        printf("<%s>@",n);

        memcpy(n,tuple->NbpName.ZoneName,tuple->NbpName.ZoneNameLen);
        n[tuple->NbpName.ZoneNameLen]=0;
        printf("<%s>",n);
        printf(" - ");
        printf("%04lX.%02X.%02X\n",tuple->Address.Network,tuple->Address.Node,tuple->Address.Socket);
    }
}


int LookupZone(int s)
{
    char buf[1024*12];
    DWORD wsiz;
    char *zone;
    int    r,i;

    wsiz=sizeof(buf);
    r = getsockopt(s, SOL_APPLETALK, SO_LOOKUP_ZONES,(char*)buf,&wsiz);
    if(r != NO_ERROR){
        printf("getsockopt:error = %d\n", WSAGetLastError());
        return -1;
    }
    i=((PWSH_LOOKUP_ZONES)buf)->NoZones;
    zone=buf+sizeof(WSH_LOOKUP_ZONES);
    while(i-->0){
        printf("Zone=%s\n",zone);
        LookupServer(s,zone);
        zone+=strlen(zone)+1;
    }
    return 0;
}

int create_socket()
{
    int    s;
    int    r;
    SOCKADDR_AT ataddress;

    s=socket(AF_APPLETALK, SOCK_DGRAM, DDPPROTO_ATP);
    if(s == INVALID_SOCKET){
        printf("Open Socket: Error = %ld\n", WSAGetLastError());
        return -1;
    }
    ataddress.sat_socket = 0;
    ataddress.sat_family = AF_APPLETALK;
    r = bind(s, (struct sockaddr *)&ataddress, sizeof(SOCKADDR_AT));
    if(r < 0){
        printf("Bind:Error = %d\n", WSAGetLastError());
        closesocket(s);
        return -1;
    }
    return s;
}

main()
{
    WSADATA WsaData;
    int    r,s;

    r = WSAStartup(0x0101, &WsaData);
    if (r == SOCKET_ERROR){
        printf("Startup failed!\n");
        exit(1);
    }
    if((s=create_socket())<0){
        printf("Create Socket failed!\n");
    }
    else{
        LookupZone(s);
        closesocket(s);
    }
    WSACleanup();
    exit(0);
}
#endif