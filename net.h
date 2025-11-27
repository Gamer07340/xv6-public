#ifndef NET_H
#define NET_H

#include "types.h"

// Network buffer management
struct mbuf {
  struct mbuf *next;  // next mbuf in the chain
  char *head;         // start of data
  int len;            // length of data
  char buf[2048];     // buffer storage
};

struct mbuf* mbufalloc(int headroom);
void mbuffree(struct mbuf *m);
void mbufput(struct mbuf *m, int len);
void mbufpull(struct mbuf *m, int len);
void mbufpush(struct mbuf *m, int len);
void mbuftrim(struct mbuf *m, int len);

// Endianness conversion
#define htons(x) ((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8))
#define ntohs(x) htons(x)
#define htonl(x) ((((x) & 0xff) << 24) | \
                  (((x) & 0xff00) << 8) | \
                  (((x) & 0xff0000) >> 8) | \
                  (((x) & 0xff000000) >> 24))
#define ntohl(x) htonl(x)

// Ethernet
#define ETHADDR_LEN 6
#define ETHTYPE_IP  0x0800
#define ETHTYPE_ARP 0x0806

struct eth {
  uchar dhost[ETHADDR_LEN];
  uchar shost[ETHADDR_LEN];
  ushort type;
} __attribute__((packed));

// ARP
#define ARP_HRD_ETHER 1
#define ARP_PRO_IP    0x0800
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

struct arp {
  ushort hrd; // Hardware type
  ushort pro; // Protocol type
  uchar hln;  // Hardware address length
  uchar pln;  // Protocol address length
  ushort op;  // Operation
  uchar sha[ETHADDR_LEN]; // Sender hardware address
  uint sip;   // Sender IP address
  uchar tha[ETHADDR_LEN]; // Target hardware address
  uint tip;   // Target IP address
} __attribute__((packed));

// IP
#define IP_VER_IHL(v, l) (((v) << 4) | (l))
#define IP_VER(v) ((v) >> 4)
#define IP_IHL(v) ((v) & 0xF)
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

struct ip {
  uchar vhl;    // Version and Header Length
  uchar tos;    // Type of Service
  ushort len;   // Total Length
  ushort id;    // Identification
  ushort off;   // Fragment Offset
  uchar ttl;    // Time to Live
  uchar p;      // Protocol
  ushort sum;   // Checksum
  uint src;     // Source IP
  uint dst;     // Destination IP
} __attribute__((packed));

// UDP
struct udp {
  ushort sport; // Source Port
  ushort dport; // Destination Port
  ushort len;   // Length
  ushort sum;   // Checksum
} __attribute__((packed));

// TCP
struct tcp {
  ushort sport; // Source Port
  ushort dport; // Destination Port
  uint seq;     // Sequence Number
  uint ack;     // Acknowledgment Number
  uchar off;    // Data Offset
  uchar flags;  // Flags
  ushort win;   // Window Size
  ushort sum;   // Checksum
  ushort urp;   // Urgent Pointer
} __attribute__((packed));

// ICMP
#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8

struct icmp {
  uchar type;
  uchar code;
  ushort checksum;
  ushort id;
  ushort seq;
} __attribute__((packed));

void net_rx(struct mbuf *m);
void net_tx(struct mbuf *m);

// Socket types
#define SOCK_DGRAM 1  // UDP
#define SOCK_STREAM 2 // TCP
#define SOCK_RAW 3    // Raw IP

// Socket structure
struct socket {
  int used;
  int type;
  uint local_ip;
  ushort local_port;
  uint remote_ip;
  ushort remote_port;
  struct mbuf *rxq;  // Receive queue
  int state;         // For TCP
  
  // TCP-specific fields
  uint snd_una;      // Send unacknowledged
  uint snd_nxt;      // Send next
  uint snd_wnd;      // Send window
  uint rcv_nxt;      // Receive next
  uint rcv_wnd;      // Receive window
  uint iss;          // Initial send sequence
  uint irs;          // Initial receive sequence
};

// TCP States (RFC 793)
#define TCP_CLOSED       0
#define TCP_LISTEN       1
#define TCP_SYN_SENT     2
#define TCP_SYN_RECEIVED 3
#define TCP_ESTABLISHED  4
#define TCP_FIN_WAIT_1   5
#define TCP_FIN_WAIT_2   6
#define TCP_CLOSE_WAIT   7
#define TCP_CLOSING      8
#define TCP_LAST_ACK     9
#define TCP_TIME_WAIT    10

// TCP Flags
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

// TCP Options
#define TCP_OPT_EOL  0
#define TCP_OPT_NOP  1
#define TCP_OPT_MSS  2

// DNS structures
#define DNS_PORT 53
#define DNS_TYPE_A 1
#define DNS_CLASS_IN 1

struct dns_header {
  ushort id;
  ushort flags;
  ushort qdcount;
  ushort ancount;
  ushort nscount;
  ushort arcount;
} __attribute__((packed));

#endif
