#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "net.h"

#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

extern void* memmove(void*, const void*, uint);
void e1000_transmit(struct mbuf *m);
void ip_rx(struct mbuf *m);
void arp_rx(struct mbuf *m);
void icmp_rx(struct mbuf *m);
void udp_rx(struct mbuf *m);
void tcp_rx(struct mbuf *m);
void tcp_send(struct socket *sock, uchar flags, char *data, int len);
void net_tx_udp(uint dip, ushort sport, ushort dport, struct mbuf *payload);

ushort in_cksum(ushort *addr, int len) {
  int nleft = len;
  uint sum = 0;
  ushort *w = addr;
  ushort answer = 0;

  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  if (nleft == 1) {
    *(uchar *)(&answer) = *(uchar *)w;
    sum += answer;
  }

  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);
  answer = ~sum;
  return answer;
}

struct mbuf* mbufalloc(int headroom) {
  struct mbuf *m;
  if (headroom > 2048)
    return 0;
  m = (struct mbuf*)kalloc();
  if (m == 0)
    return 0;
  m->next = 0;
  m->head = (char*)m->buf + headroom;
  m->len = 0;
  return m;
}

void mbuffree(struct mbuf *m) {
  kfree((char*)m);
}

void mbufput(struct mbuf *m, int len) {
  m->len += len;
}

void mbufpull(struct mbuf *m, int len) {
  m->len -= len;
  m->head += len;
}

void mbufpush(struct mbuf *m, int len) {
  m->head -= len;
  m->len += len;
}

void mbuftrim(struct mbuf *m, int len) {
  m->len = len;
}

// ARP Table
#define ARP_TABLE_SIZE 10
struct {
  uint ip;
  uchar mac[ETHADDR_LEN];
  int valid;
} arp_table[ARP_TABLE_SIZE];

void arp_init(void) {
  int i;
  for(i = 0; i < ARP_TABLE_SIZE; i++)
    arp_table[i].valid = 0;
}

void arp_rx(struct mbuf *m) {
  struct arp *arpheader;
  struct eth *ethhdr;
  
  if (m->len < sizeof(*arpheader)) {
    mbuffree(m);
    return;
  }
  
  arpheader = (struct arp*)m->head;
  ethhdr = (struct eth*)(m->head - sizeof(struct eth));
  
  if (ntohs(arpheader->hrd) != ARP_HRD_ETHER ||
      ntohs(arpheader->pro) != ARP_PRO_IP ||
      arpheader->hln != ETHADDR_LEN ||
      arpheader->pln != sizeof(uint)) {
    mbuffree(m);
    return;
  }
  
  if (ntohs(arpheader->op) == ARP_OP_REQUEST) {
    // Handle ARP Request
    // For now, we assume we are 10.0.2.15 (QEMU default)
    
    // Send ARP Reply (Swap src/dst)
    struct mbuf *reply = mbufalloc(sizeof(struct eth) + sizeof(struct arp));
    if (!reply) {
      mbuffree(m);
      return;
    }
    
    struct eth *eth = (struct eth*)reply->head;
    struct arp *arp = (struct arp*)(reply->head + sizeof(struct eth));
    
    // Fill Ethernet Header
    memmove(eth->dhost, ethhdr->shost, ETHADDR_LEN);
    uchar mymac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56}; 
    memmove(eth->shost, mymac, ETHADDR_LEN);
    eth->type = htons(ETHTYPE_ARP);
    
    // Fill ARP Header
    arp->hrd = htons(ARP_HRD_ETHER);
    arp->pro = htons(ARP_PRO_IP);
    arp->hln = ETHADDR_LEN;
    arp->pln = sizeof(uint);
    arp->op = htons(ARP_OP_REPLY);
    memmove(arp->sha, mymac, ETHADDR_LEN);
    arp->sip = arpheader->tip; // We are the target (IP)
    memmove(arp->tha, arpheader->sha, ETHADDR_LEN);
    arp->tip = arpheader->sip;
    
    reply->len = sizeof(struct eth) + sizeof(struct arp);
    e1000_transmit(reply);
  }
  
  mbuffree(m);
}

// Socket table (defined here for use in icmp_rx)
#define MAX_SOCKETS 16
extern struct socket sockets[MAX_SOCKETS];

void icmp_rx(struct mbuf *m) {
  struct icmp *icmpheader;
  struct ip *iphdr;
  struct eth *ethhdr;
  int i;
  
  if (m->len < sizeof(*icmpheader)) {
    mbuffree(m);
    return;
  }
  
  icmpheader = (struct icmp*)m->head;
  iphdr = (struct ip*)(m->head - sizeof(struct ip));
  
  // Debug: print ICMP type
  cprintf("ICMP: type=%d code=%d id=%d seq=%d from %x\n", 
          icmpheader->type, icmpheader->code, 
          ntohs(icmpheader->id), ntohs(icmpheader->seq),
          iphdr->src);
  
  // Check for raw sockets waiting for ICMP replies
  if (icmpheader->type == ICMP_ECHO_REPLY) {
    // Find matching raw socket (match by remote IP or any raw socket)
    for (i = 0; i < MAX_SOCKETS; i++) {
      if (sockets[i].used && sockets[i].type == SOCK_RAW) {
        cprintf("Found raw socket %d, remote_ip=%x, src=%x\n", 
                i, sockets[i].remote_ip, iphdr->src);
        
        // Match by remote IP or accept any if remote_ip is 0
        if (sockets[i].remote_ip == 0 || sockets[i].remote_ip == iphdr->src) {
          // Queue packet for socket
          if (sockets[i].rxq)
            mbuffree(sockets[i].rxq);
          sockets[i].rxq = m;
          cprintf("Queued ICMP reply to socket %d\n", i);
          return; // Don't free m
        }
      }
    }
    cprintf("No matching raw socket found\n");
  }
  
  if (icmpheader->type == ICMP_ECHO_REQUEST) {
    // Send Echo Reply
    struct mbuf *reply = mbufalloc(sizeof(struct eth) + sizeof(struct ip) + m->len);
    if (!reply) {
      mbuffree(m);
      return;
    }
    
    struct eth *eth_reply = (struct eth*)reply->head;
    struct ip *ip_reply = (struct ip*)(reply->head + sizeof(struct eth));
    struct icmp *icmp_reply = (struct icmp*)(reply->head + sizeof(struct eth) + sizeof(struct ip));
    
    // Copy data payload
    memmove((char*)icmp_reply + sizeof(struct icmp), (char*)icmpheader + sizeof(struct icmp), m->len - sizeof(struct icmp));
    
    // Fill ICMP Header
    icmp_reply->type = ICMP_ECHO_REPLY;
    icmp_reply->code = 0;
    icmp_reply->id = icmpheader->id;
    icmp_reply->seq = icmpheader->seq;
    
    // Calculate Checksum
    icmp_reply->checksum = 0;
    icmp_reply->checksum = in_cksum((ushort*)(void*)icmp_reply, m->len);
    
    // Fill IP Header
    iphdr = (struct ip*)(m->head - sizeof(struct ip));
    
    ip_reply->vhl = IP_VER_IHL(4, 5);
    ip_reply->tos = 0;
    ip_reply->len = htons(sizeof(struct ip) + m->len);
    ip_reply->id = 0;
    ip_reply->off = 0;
    ip_reply->ttl = 64;
    ip_reply->p = IP_PROTO_ICMP;
    ip_reply->src = iphdr->dst; // We are src now
    ip_reply->dst = iphdr->src; // They are dst
    ip_reply->sum = 0;
    ip_reply->sum = in_cksum((ushort*)(void*)ip_reply, sizeof(struct ip));
    
    // Fill Ethernet Header
    ethhdr = (struct eth*)(m->head - sizeof(struct ip) - sizeof(struct eth));
    memmove(eth_reply->dhost, ethhdr->shost, ETHADDR_LEN);
    uchar mymac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
    memmove(eth_reply->shost, mymac, ETHADDR_LEN);
    eth_reply->type = htons(ETHTYPE_IP);
    
    reply->len = sizeof(struct eth) + sizeof(struct ip) + m->len;
    e1000_transmit(reply);
  }
  
  mbuffree(m);
}

void ip_rx(struct mbuf *m) {
  struct ip *iphdr;
  
  if (m->len < sizeof(*iphdr)) {
    mbuffree(m);
    return;
  }
  
  iphdr = (struct ip*)m->head;
  
  if (IP_VER(iphdr->vhl) != 4) {
    mbuffree(m);
    return;
  }
  
  // Checksum validation (TODO)
  
  mbufpull(m, sizeof(struct ip)); // Advance to transport header
  
  // Dispatch based on protocol
  if (iphdr->p == IP_PROTO_ICMP) {
    icmp_rx(m);
  } else if (iphdr->p == IP_PROTO_UDP) {
    udp_rx(m);
  } else if (iphdr->p == IP_PROTO_TCP) {
    tcp_rx(m);
  } else {
    mbuffree(m);
  }
}

void icmp_send(uint dip, ushort id, ushort seq, char *data, int len) {
  struct mbuf *m = mbufalloc(sizeof(struct eth) + sizeof(struct ip) + sizeof(struct icmp) + len);
  if (!m) return;
  
  cprintf("ICMP send: to %x id=%d seq=%d len=%d\n", dip, id, seq, len);
  
  struct eth *eth = (struct eth*)m->head;
  struct ip *ip = (struct ip*)(m->head + sizeof(struct eth));
  struct icmp *icmp = (struct icmp*)(m->head + sizeof(struct eth) + sizeof(struct ip));
  
  // Copy data
  if (data && len > 0) {
    memmove((char*)icmp + sizeof(struct icmp), data, len);
  }
  
  // Fill ICMP header
  icmp->type = ICMP_ECHO_REQUEST;
  icmp->code = 0;
  icmp->id = htons(id);
  icmp->seq = htons(seq);
  icmp->checksum = 0;
  icmp->checksum = in_cksum((ushort*)(void*)icmp, sizeof(struct icmp) + len);
  
  // Fill IP header
  ip->vhl = IP_VER_IHL(4, 5);
  ip->tos = 0;
  ip->len = htons(sizeof(struct ip) + sizeof(struct icmp) + len);
  ip->id = 0;
  ip->off = 0;
  ip->ttl = 64;
  ip->p = IP_PROTO_ICMP;
  ip->src = htonl(0x0a000215); // 10.0.2.15
  ip->dst = dip;
  ip->sum = 0;
  ip->sum = in_cksum((ushort*)(void*)ip, sizeof(struct ip));
  
  // Fill Ethernet header
  uchar broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  memmove(eth->dhost, broadcast, ETHADDR_LEN);
  uchar mymac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
  memmove(eth->shost, mymac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);
  
  m->len = sizeof(struct eth) + sizeof(struct ip) + sizeof(struct icmp) + len;
  e1000_transmit(m);
}

// Socket table
struct socket sockets[MAX_SOCKETS];

uint tcp_seq = 1000; // Global sequence number

void socket_init(void) {
  int i;
  for (i = 0; i < MAX_SOCKETS; i++) {
    sockets[i].used = 0;
    sockets[i].state = TCP_CLOSED;
  }
}

// TCP checksum calculation (includes pseudo-header)
ushort tcp_checksum(struct ip *iphdr, struct tcp *tcphdr, int tcp_len) {
  uint sum = 0;
  ushort *ptr;
  int i;
  
  // Pseudo-header
  sum += (iphdr->src >> 16) & 0xFFFF;
  sum += iphdr->src & 0xFFFF;
  sum += (iphdr->dst >> 16) & 0xFFFF;
  sum += iphdr->dst & 0xFFFF;
  sum += htons(IP_PROTO_TCP);
  sum += htons(tcp_len);
  
  // TCP header and data
  ptr = (ushort*)tcphdr;
  for (i = 0; i < tcp_len / 2; i++) {
    sum += ptr[i];
  }
  if (tcp_len & 1) {
    sum += ((uchar*)tcphdr)[tcp_len - 1];
  }
  
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }
  
  return ~sum;
}

void tcp_send(struct socket *sock, uchar flags, char *data, int len) {
  struct mbuf *m = mbufalloc(sizeof(struct eth) + sizeof(struct ip) + sizeof(struct tcp) + len);
  if (!m) return;
  
  struct eth *eth = (struct eth*)m->head;
  struct ip *ip = (struct ip*)(m->head + sizeof(struct eth));
  struct tcp *tcp = (struct tcp*)(m->head + sizeof(struct eth) + sizeof(struct ip));
  
  // Copy data
  if (data && len > 0) {
    memmove((char*)tcp + sizeof(struct tcp), data, len);
  }
  
  // Fill TCP header
  tcp->sport = htons(sock->local_port);
  tcp->dport = htons(sock->remote_port);
  tcp->seq = htonl(sock->snd_nxt);
  tcp->ack = (flags & TCP_ACK) ? htonl(sock->rcv_nxt) : 0;
  tcp->off = (sizeof(struct tcp) / 4) << 4; // Data offset in 32-bit words
  tcp->flags = flags;
  tcp->win = htons(8192); // Window size
  tcp->sum = 0;
  tcp->urp = 0;
  
  // Fill IP header
  ip->vhl = IP_VER_IHL(4, 5);
  ip->tos = 0;
  ip->len = htons(sizeof(struct ip) + sizeof(struct tcp) + len);
  ip->id = 0;
  ip->off = 0;
  ip->ttl = 64;
  ip->p = IP_PROTO_TCP;
  ip->src = sock->local_ip;
  ip->dst = sock->remote_ip;
  ip->sum = 0;
  ip->sum = in_cksum((ushort*)(void*)ip, sizeof(struct ip));
  
  // Calculate TCP checksum
  tcp->sum = tcp_checksum(ip, tcp, sizeof(struct tcp) + len);
  
  // Fill Ethernet header
  uchar broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  memmove(eth->dhost, broadcast, ETHADDR_LEN);
  uchar mymac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
  memmove(eth->shost, mymac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);
  
  m->len = sizeof(struct eth) + sizeof(struct ip) + sizeof(struct tcp) + len;
  
  // Update sequence number
  if (flags & TCP_SYN) sock->snd_nxt++;
  if (flags & TCP_FIN) sock->snd_nxt++;
  if (len > 0) sock->snd_nxt += len;
  
  e1000_transmit(m);
}

void tcp_rx(struct mbuf *m) {
  struct tcp *tcphdr;
  struct ip *iphdr;
  int i;
  
  if (m->len < sizeof(*tcphdr)) {
    mbuffree(m);
    return;
  }
  
  tcphdr = (struct tcp*)m->head;
  iphdr = (struct ip*)(m->head - sizeof(struct ip));
  
  ushort dport = ntohs(tcphdr->dport);
  ushort sport = ntohs(tcphdr->sport);
  uint seq = ntohl(tcphdr->seq);
  uint ack = ntohl(tcphdr->ack);
  uchar flags = tcphdr->flags;
  
  // Find matching socket
  for (i = 0; i < MAX_SOCKETS; i++) {
    if (!sockets[i].used || sockets[i].type != SOCK_STREAM)
      continue;
    
    if (sockets[i].local_port == dport &&
        (sockets[i].remote_port == 0 || sockets[i].remote_port == sport) &&
        (sockets[i].remote_ip == 0 || sockets[i].remote_ip == iphdr->src)) {
      
      struct socket *sock = &sockets[i];
      
      // TCP State Machine (RFC 793)
      switch (sock->state) {
        case TCP_CLOSED:
          // Send RST
          mbuffree(m);
          return;
          
        case TCP_LISTEN:
          if (flags & TCP_SYN) {
            sock->remote_ip = iphdr->src;
            sock->remote_port = sport;
            sock->rcv_nxt = seq + 1;
            sock->irs = seq;
            sock->iss = tcp_seq++;
            sock->snd_nxt = sock->iss + 1;
            sock->state = TCP_SYN_RECEIVED;
            tcp_send(sock, TCP_SYN | TCP_ACK, 0, 0);
          }
          break;
          
        case TCP_SYN_SENT:
          if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
            sock->rcv_nxt = seq + 1;
            sock->irs = seq;
            sock->snd_una = ack;
            sock->state = TCP_ESTABLISHED;
            tcp_send(sock, TCP_ACK, 0, 0);
          } else if (flags & TCP_SYN) {
            sock->rcv_nxt = seq + 1;
            sock->irs = seq;
            sock->state = TCP_SYN_RECEIVED;
            tcp_send(sock, TCP_ACK, 0, 0);
          }
          break;
          
        case TCP_SYN_RECEIVED:
          if (flags & TCP_ACK) {
            sock->snd_una = ack;
            sock->state = TCP_ESTABLISHED;
          }
          break;
          
        case TCP_ESTABLISHED:
          if (flags & TCP_FIN) {
            sock->rcv_nxt = seq + 1;
            sock->state = TCP_CLOSE_WAIT;
            tcp_send(sock, TCP_ACK, 0, 0);
          } else if (flags & TCP_ACK) {
            sock->snd_una = ack;
            
            // Handle data
            int data_len = m->len - ((tcphdr->off >> 4) * 4);
            if (data_len > 0 && seq == sock->rcv_nxt) {
              mbufpull(m, (tcphdr->off >> 4) * 4);
              
              // Queue data
              if (sock->rxq)
                mbuffree(sock->rxq);
              sock->rxq = m;
              sock->rcv_nxt += data_len;
              
              // Send ACK
              tcp_send(sock, TCP_ACK, 0, 0);
              return; // Don't free m
            }
          }
          break;
          
        case TCP_FIN_WAIT_1:
          if (flags & TCP_FIN) {
            sock->rcv_nxt = seq + 1;
            tcp_send(sock, TCP_ACK, 0, 0);
            if (flags & TCP_ACK) {
              sock->state = TCP_TIME_WAIT;
            } else {
              sock->state = TCP_CLOSING;
            }
          } else if (flags & TCP_ACK) {
            sock->state = TCP_FIN_WAIT_2;
          }
          break;
          
        case TCP_FIN_WAIT_2:
          if (flags & TCP_FIN) {
            sock->rcv_nxt = seq + 1;
            tcp_send(sock, TCP_ACK, 0, 0);
            sock->state = TCP_TIME_WAIT;
          }
          break;
          
        case TCP_CLOSE_WAIT:
          // Application should close
          break;
          
        case TCP_CLOSING:
          if (flags & TCP_ACK) {
            sock->state = TCP_TIME_WAIT;
          }
          break;
          
        case TCP_LAST_ACK:
          if (flags & TCP_ACK) {
            sock->state = TCP_CLOSED;
            sock->used = 0;
          }
          break;
          
        case TCP_TIME_WAIT:
          // Wait for 2MSL, then close
          sock->state = TCP_CLOSED;
          sock->used = 0;
          break;
      }
      
      mbuffree(m);
      return;
    }
  }
  
  mbuffree(m);
}

void udp_rx(struct mbuf *m) {
  struct udp *udphdr;
  struct ip *iphdr;
  int i;
  
  if (m->len < sizeof(*udphdr)) {
    mbuffree(m);
    return;
  }
  
  udphdr = (struct udp*)m->head;
  iphdr = (struct ip*)(m->head - sizeof(struct ip));
  
  ushort dport = ntohs(udphdr->dport);
  ushort sport = ntohs(udphdr->sport);
  
  // Find matching socket
  for (i = 0; i < MAX_SOCKETS; i++) {
    if (sockets[i].used && sockets[i].type == SOCK_DGRAM && 
        sockets[i].local_port == dport) {
      // Queue packet for socket
      mbufpull(m, sizeof(struct udp));
      
      // Simple queue - just store one packet for now
      if (sockets[i].rxq)
        mbuffree(sockets[i].rxq);
      sockets[i].rxq = m;
      sockets[i].remote_ip = iphdr->src;
      sockets[i].remote_port = sport;
      return;
    }
  }
  
  mbuffree(m);
}

void net_tx_udp(uint dip, ushort sport, ushort dport, struct mbuf *payload) {
  struct mbuf *m = mbufalloc(sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp));
  if (!m) {
    mbuffree(payload);
    return;
  }
  
  struct eth *eth = (struct eth*)m->head;
  struct ip *ip = (struct ip*)(m->head + sizeof(struct eth));
  struct udp *udp = (struct udp*)(m->head + sizeof(struct eth) + sizeof(struct ip));
  
  // Copy payload
  memmove((char*)udp + sizeof(struct udp), payload->head, payload->len);
  
  // Fill UDP header
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->len = htons(sizeof(struct udp) + payload->len);
  udp->sum = 0;
  
  // Fill IP header
  ip->vhl = IP_VER_IHL(4, 5);
  ip->tos = 0;
  ip->len = htons(sizeof(struct ip) + sizeof(struct udp) + payload->len);
  ip->id = 0;
  ip->off = 0;
  ip->ttl = 64;
  ip->p = IP_PROTO_UDP;
  ip->src = htonl(0x0a000215); // 10.0.2.15
  ip->dst = dip;
  ip->sum = 0;
  ip->sum = in_cksum((ushort*)(void*)ip, sizeof(struct ip));
  
  // Fill Ethernet header
  // TODO: ARP lookup for MAC address
  uchar broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  memmove(eth->dhost, broadcast, ETHADDR_LEN);
  uchar mymac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
  memmove(eth->shost, mymac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);
  
  m->len = sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp) + payload->len;
  mbuffree(payload);
  e1000_transmit(m);
}

void net_rx(struct mbuf *m) {
  struct eth *ethhdr;
  
  if (m->len < sizeof(*ethhdr)) {
    mbuffree(m);
    return;
  }
  
  ethhdr = (struct eth*)m->head;
  mbufpull(m, sizeof(*ethhdr));
  
  ushort type = ntohs(ethhdr->type);
  if (type == ETHTYPE_IP) {
    ip_rx(m);
  } else if (type == ETHTYPE_ARP) {
    arp_rx(m);
  } else {
    mbuffree(m);
  }
}
