/*
 *  RAWtcpip.h  
 *     some direct low-level IP stuff for RAW sockets
 *     collected by, yours truely, 
 *     David Gravereaux <davygrvy@pobox.com>
 *
 *     NOTE: #define BIG_ENDIAN for NON little-endian (Intel architecture) 
 *           specific bit fields.
 */

/* save bit alignment */
#pragma pack( push, before )

/* sets bit alignment of structures. We want maximum squish */
#pragma pack(1)


/*
 * IP header.
 * Per RFC 791, September, 1981.
 */
#define IPVERSION  4
typedef struct iphdr {
#if BIG_ENDIAN
        u_char  ip_v:4,                 /* version. we are version 4. IPv6 is 6 */
                ip_hl:4;                /* header length given in # of 32-bit words. min is 5 */
#else
        u_char  ip_hl:4,                /* header length given in # of 32-bit words. min is 5 */
                ip_v:4;                 /* version. we are version 4. IPv6 is 6 */
#endif
        union tos {
            u_char ip_TOS;              /* type of service */
            struct tos_bit {
#if BIG_ENDIAN
                u_char ip_tos_prc:3,    /* precedence */
                       ip_tos_d:1,      /* frame delay */
                       ip_tos_t:1,      /* frame throughput */
                       ip_tos_r:1,      /* frame reliability */
                       ip_tos_rsv:2;    /* reserved -- set to zero */
#else
                u_char ip_tos_rsv:2,    /* reserved -- set to zero */
                       ip_tos_r:1,      /* frame reliability */
                       ip_tos_t:1,      /* frame throughput */
                       ip_tos_d:1,      /* frame delay */
                       ip_tos_prc:3;    /* precedence */
#endif
            } tos_bit;
#define IP_TOS_NETCTRL    0x7           /* Network Control */
#define IP_TOS_INETCTRL   0x6           /* Internetwork Control */
#define IP_TOS_CRITICECP  0x5           /* CRITIC/ECP */
#define IP_TOS_FLASHOVR   0x4           /* Falsh Override */
#define IP_TOS_FLASH      0x3           /* Flash */
#define IP_TOS_IMMEDIATE  0x2           /* Immediate */
#define IP_TOS_PRIORITY   0x1           /* Priority */
#define ip_tos      tos.ip_TOS
#define ip_tos_resv tos.tos_bit.ip_tos_rsv
#define ip_tos_R    tos.tos_bit.ip_tos_r
#define ip_tos_T    tos.tos_bit.ip_tos_t
#define ip_tos_D    tos.tos_bit.ip_tos_d
#define ip_tos_prec tos.tos_bit.ip_tos_prc
        } tos;
        u_short ip_len;                 /* length of header+data given in # of octets */
        u_short ip_id;                  /* identification (optional, done by kernel if not specified) */
        union frag {
            u_short ip_frag;             /* entire Fragment field */
#define ip_frg frag.ip_frag
            struct ff {
#if BIG_ENDIAN
                u_char rsv:1,           /* reserved -- set to zero */
                       dflg:1,          /* don't fragment flag */
                       mflg:1;          /* more fragment flag */
                u_short ip_off:13;      /* fragment offset field */
#define ip_foff frag.ff.ip_off
#else
                u_char off1:5,          /* fragment offset field - hibyte */
                       mflg:1,          /* more fragment flag */
                       dflg:1,          /* don't fragment flag */
                       rsv:1,           /* reserved -- set to zero */
                       off2:8;          /* fragment offset field - lobyte */
#define ip_foff(Z) MAKEWORD(Z->frag.ff.off1, Z->frag.ff.off2)
#endif
            } ff;
#define ip_df frag.ff.dflg
#define ip_mf frag.ff.mflg
        } frag;
        u_char  ip_ttl;                 /* time to live */
        u_char  ip_p;                   /* protocol of the data following */
        u_short ip_chksum;              /* checksum of the ip header (optional, done by kernel anyways) */
        struct in_addr ip_src, ip_dst;  /* source and destination addresses */
} IPHDR, *PIPHDR, FAR *LPIPHDR;
#define IP_HDR_LEN  sizeof(IPHDR)


#define MAX_ROUTE   9
/*
 * IP options structure
 */
typedef struct ipopt {
#if BIG_ENDIAN
    u_char ipopt_flag:1,
           ipopt_class:2,
           ipopt_type:5;
#else
    u_char ipopt_type:5,
           ipopt_class:2,
           ipopt_flag:1;
#endif
    u_char ipopt_len;
    u_char ipopt_ptr;
    union ipopt_data {
        struct in_addr raddr[MAX_ROUTE];        /* Record Routing */
#define ipopt_raddr(Z) ipopt_data.raddr[Z]
        struct ts {                             /* TimeStamp */
#if BIG_ENDIAN
            u_char oflw:4,                      /* TimeStamp overflow count */
                   flg:4;                       /* TimeStamp flags */
#else
            u_char flg:4,                       /* TimeStamp flags */
                   oflw:4;                      /* TimeStamp overflow count */
#endif
            union tsdata {
                u_long time[MAX_ROUTE];
                struct in_addr tsaddr[MAX_ROUTE];
            } tsdata;
        } ts;
#define ipopt_tsflg ipopt_data.ts.flg
#define ipopt_tsoflw ipopt_data.ts.oflw
#define ipopt_ts(Z) ipopt_data.ts.tsdata.time[Z]
#define ipopt_tsaddr(Z) ipopt_data.ts.tsdata.tsaddr[Z]
        u_long sid;                           /* Stream ID */
#define ipopt_sid ipopt_data.sid
    } ipopt_data;
} IPOPT, *PIPOPT, FAR *LPIPOPT;

#define IPOPT_END       0
#define IPOPT_NOOP      1
// Security option isn't used on the global internet
/* #define IPOPT_SEC       2 */
#define IPOPT_LSRR      3
#define IPOPT_TIMESTAMP 4
#define IPOPT_RR        7
// Stream Identifier is obsolete
/* #define IPOPT_SID       8 */
#define IPOPT_SSRR      9

#define IPOPT_OPTVAL 0
#define IPOPT_OLEN   1
#define IPOPT_OFFSET 2
#define IPOPT_MINOFF 4
#define IPOPT_MAXLEN 40
#define IPOPT_NOP IPOPT_NOOP
#define IPOPT_EOL IPOPT_END
#define IPOPT_TS  IPOPT_TIMESTAMP

#define	IPOPT_TS_TSONLY	    0       /* timestamps only */
#define	IPOPT_TS_TSANDADDR  1       /* timestamps and addresses */
#define	IPOPT_TS_PRESPEC    3       /* pre-specified modules only */



/*
 * TCP header.
 * Per RFC 793, September, 1981.
 */
typedef	u_long tcp_seq;
typedef struct tcphdr {
  u_short  th_sport;     /* source port */
  u_short  th_dport;     /* destination port */
  tcp_seq  th_seqnum;       /* sequence number */
  tcp_seq  th_acknum;       /* acknowledgement number */
#if BIG_ENDIAN
  u_char   th_doff:4,     /* data offset */
           th_x2:4;      /* reserved -- set to zero */
#else
  u_char   th_x2:4,      /* reserved -- set to zero */
           th_doff:4;     /* data offset */
#endif
  union flg {
      u_char flags;
#define th_flags flg.flags
      struct bit {
#if BIG_ENDIAN
          u_char rsv:2,     /* reserved -- set to zero */
                 urg:1,     /* Urgent flag */
                 ack:1,     /* acknowledgement flag */
                 push:1,    /*  */
                 rst:1,     /*  */
                 syn:1,     /*  */
                 fin:1;     /*  */
#else
          u_char fin:1,
                 syn:1,
                 rst:1,
                 push:1,
                 ack:1,	    /* acknowledgement flag */
                 urg:1,     /* Urgent flag */
                 rsv:2;     /* reserved -- set to zero */
#endif
#define th_fin flg.bit.fin
#define th_syn flg.bit.syn
#define th_rst flg.bit.rst
#define th_push flg.bit.push
#define th_ack flg.bit.ack
#define th_urg flg.bit.urg
      } bit;
  } flg;
  u_short  th_win;	/* window */
  u_short  th_chksum;	/* checksum */
  u_short  th_urp;	/* urgent pointer */
} TCPHDR, *PTCPHDR, FAR *LPTCPHDR;
#define TCP_HDR_LEN     sizeof(TCPHDR)

/* TCP socket options */
#define TCP_MAXSEG		2	/* Limit MSS */
#define TCP_CORK		3	/* Never send partially complete segments */
#define TCP_KEEPIDLE		4	/* Start keeplives after this period */
#define TCP_KEEPINTVL		5	/* Interval between keepalives */
#define TCP_KEEPCNT		6	/* Number of keepalives before death */
#define TCP_SYNCNT		7	/* Number of SYN retransmits */
#define TCP_LINGER2		8	/* Life time of orphaned FIN-WAIT-2 state */
#define TCP_DEFER_ACCEPT	9	/* Wake up listener only when data arrive */
#define TCP_WINDOW_CLAMP	10	/* Bound advertised window */
#define TCP_INFO		11	/* Information about this connection. */
#define TCP_QUICKACK		12	/* Block/reenable quick acks */

#define TCPI_OPT_TIMESTAMPS	1
#define TCPI_OPT_SACK		2
#define TCPI_OPT_WSCALE		4
#define TCPI_OPT_ECN		8

struct tcpopt_mss {
    u_char kind;
    u_char len;
    u_short mss;
    u_long pad1;
//    u_short pad2;
};

/*
 * UDP header.
 * per RFC 768 
 */
typedef struct udphdr {
  u_short uh_sport;         /* source port (optional on send) */
  u_short uh_dport;         /* destination port       */
  u_short uh_len;           /* UDP header length in # of octets */
  u_short uh_chksum;        /* check sum (optional on send)   */
} UDPHDR, *PUDPHDR, FAR *LPUDPHDR;
#define UDP_HDR_LEN     sizeof(UDPHDR)


/*
 * IGMP header.
 * as per RFC 1112
 */
typedef struct igmphdr {
#if BIG_ENDIAN
    u_char  igmp_v:4,               /* version. should be 1; 0 is obselete */
            igmp_type:4;            /* type of message */
#else
    u_char  igmp_type:4,            /* type of message */
            igmp_v:4;               /* version. should be 1; 0 is obselete */
#endif
    u_char  igmp_resv;              /* reserved -- set to zero */
    u_short igmp_chk;               /* checksum of IGMP header */
    struct in_addr igmp_gaddr;      /* group address */
} IGMPHDR, FAR * LPIGMPHDR;
#define IGMP_HDR_LEN     sizeof(IGMPHDR)


/*
 * ICMP header.
 */
typedef struct icmphdr {
    u_char	icmp_type;      /* type of message, see below */
    u_char	icmp_code;      /* type sub code */
    u_short	icmp_chksum;    /* chksum of header and payload */
    union {
        u_char ih_pptr;             /* ICMP_PARAMPROB */
        struct in_addr ih_gwaddr;   /* ICMP_REDIRECT */
        struct ih_idseq {
            u_short	icd_id;
            u_short	icd_seq;
        } ih_idseq;
        struct mtu {       /* RFC1191, section 4 -- MTU discovery */
            u_short mtu_void;
            u_short NextHop_MTU;
        } mtu;
        u_long ih_void;
    } icmp_hun;
#define	icmp_pptr   icmp_hun.ih_pptr
#define	icmp_gwaddr icmp_hun.ih_gwaddr
#define	icmp_id     icmp_hun.ih_idseq.icd_id
#define	icmp_seq    icmp_hun.ih_idseq.icd_seq
#define icmp_NextHop_MTU icmp_hun.mtu.NextHop_MTU
#define	icmp_void   icmp_hun.ih_void
    union {
        struct id_ts {
            time_t its_otime;
            time_t its_rtime;
            time_t its_ttime;
        } id_ts;
        struct id_ip  {
            struct iphdr idi_ip;
            /* options and then 64 bits of data */
#if BIG_ENDIAN
#define icmp_ip_foff icmp_dun.id_ip.idi_ip.frag.ff.ip_off
#else
#define icmp_ip_foff(Z) MAKEWORD(Z->icmp_dun.id_ip.idi_ip.frag.ff.off1, Z->icmp_dun.id_ip.idi_ip.frag.ff.off2)
#endif
        } id_ip;
        struct in_addr id_mask;
        char id_data[1];
    } icmp_dun;
#define	icmp_otime  icmp_dun.id_ts.its_otime
#define	icmp_rtime  icmp_dun.id_ts.its_rtime
#define	icmp_ttime  icmp_dun.id_ts.its_ttime
#define	icmp_ip     icmp_dun.id_ip.idi_ip
#define	icmp_mask   icmp_dun.id_mask
#define	icmp_data   icmp_dun.id_data
} ICMPHDR, *PICMPHDR, FAR *LPICMPHDR;

#define ICMP_HDR_LEN     sizeof(ICMPHDR)

#define	ICMP_MINLEN     8                                   /* abs minimum */
#define	ICMP_TSLEN      (8 + 3 * sizeof (time_t))           /* timestamp */
#define	ICMP_MASKLEN    12                                  /* address mask */
#define	ICMP_ADVLENMIN	(8 + sizeof (IPHDR) + 8)            /* min */
#define	ICMP_ADVLEN(p)  (8 + ((p)->icmp_ip.ip_hl << 2) + 8)
	                        /* N.B.: must separately check that ip_hl >= 5 */


/* ICMP message types */
#define ICMP_ECHOREPLY	           0      /* Echo Reply */
#define ICMP_UNREACH               3      /* Destination Unreachable */
/* Codes for ICMP_UNREACH */
#define     ICMP_UNREACH_NET       0      /* Network Unreachable */
#define     ICMP_UNREACH_HOST      1      /* Host Unreachable */
#define     ICMP_UNREACH_PROT      2      /* Protocol Unreachable */
#define     ICMP_UNREACH_PORT      3      /* Port Unreachable */
#define     ICMP_UNREACH_NEEDFRAG  4      /* Fragmentation Needed/DF set */
#define     ICMP_UNREACH_SRCFAIL   5      /* Source Route failed */
#define     ICMP_UNREACH_NONET     6      /* Destination Network Unknown */
#define     ICMP_UNREACH_NOHOST    7      /* Destination Host Unknown */
#define     ICMP_UNREACH_HOSTISO   8      /* Source Host Isolated */
#define     ICMP_UNREACH_NETPROHIB 9      /* Communication w/destination net adminstativly prohibited */
#define     ICMP_UNREACH_HOSTPROHIB 10    /* Communication w/destination host adminstativly prohibited */
#define     ICMP_UNREACH_TOSNET   11      /* Network Unreachable for Type-of-Service */
#define     ICMP_UNREACH_TOSHOST  12      /* Host Unreachable for Type-of-Service */
#define     ICMP_UNREACH_PKTFILT  13      /* Packet filtered */
#define     ICMP_UNREACH_PRECVIO  14      /* Precedence violation */
#define     ICMP_UNREACH_PRECCUT  15      /* Precedence cut off */
#define     NR_ICMP_UNREACH	      15      /* instead of hardcoding immediate value */
#define ICMP_SOURCEQUENCH          4      /* Source Quench */
#define ICMP_REDIRECT              5      /* Redirect (change route) */
/* Codes for ICMP_REDIRECT */
#define     ICMP_REDIRECT_NET      0      /* Redirect Net */
#define     ICMP_REDIRECT_HOST	   1      /* Redirect Host */
#define     ICMP_REDIRECT_TOSNET   2      /* Redirect Net for TOS */
#define     ICMP_REDIRECT_TOSHOST  3      /* Redirect Host for TOS */
#define ICMP_ECHO                  8      /* Echo Request */
#define ICMP_TIMXCEED             11      /* Time Exceeded */
/* Codes for ICMP_TIMEXCEED */
#define     ICMP_TIMXCEED_INTRANS  0      /* TTL count exceeded	in transit */
#define     ICMP_TIMXCEED_REASS    1      /* Fragment Reassembly time exceeded */
#define ICMP_PARAMPROB            12      /* Parameter Problem */
#define     ICMP_PARAMPROB_MISC    0      /* miscellaneous parameter problem */
#define     ICMP_PARAMPROB_REQOPT  2      /* Required option missing */
#define ICMP_TSTAMP               13      /* Timestamp Request */
#define ICMP_TSTAMPREPLY          14      /* Timestamp Reply */
#define ICMP_IREQ                 15      /* Information Request */
#define ICMP_IREQREPLY            16      /* Information Reply */
#define ICMP_MASKREQ              17      /* Address Mask Request */
#define ICMP_MASKREPLY            18      /* Address Mask Reply */
#define	ICMP_MAXTYPE              18

#define	ICMP_INFOTYPE(type) \
    ((type) == ICMP_ECHOREPLY || (type) == ICMP_ECHO || \
    (type) == ICMP_TSTAMP || (type) == ICMP_TSTAMPREPLY || \
    (type) == ICMP_IREQ || (type) == ICMP_IREQREPLY || \
    (type) == ICMP_MASKREQ || (type) == ICMP_MASKREPLY)



/*
 *  Standard CheckSum routine
 */
__inline u_short
in_cksum (u_short *addr, int count)
{
    register u_long sum = 0;

    /* add 16-bit words */
    while (count > 1)  {
      /*  this is the inner loop  */
      sum += *(addr++);
      count -= 2;
    }

    /* add leftover byte, if any */
    if (count > 0)
#if BIG_ENDIAN
      sum += (*(u_char *)addr) << 8;
#else
      sum += *(u_char *)addr;
#endif

    /*  Fold 32-bit sum to 16-bit  */
    while (sum >> 16)
      sum = (sum & 0xffff) + (sum >> 16);
 
   /* 
    * Return one's compliment of final sum. 
    */
    return (u_short) ~sum;
}

#pragma pack( pop, before )
