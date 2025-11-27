#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

extern struct socket sockets[];
extern void socket_init(void);
extern void net_tx_udp(uint dip, ushort sport, ushort dport, struct mbuf *payload);
extern void tcp_send(struct socket *sock, uchar flags, char *data, int len);
extern void icmp_send(uint dip, ushort id, ushort seq, char *data, int len);

int
sys_socket(void)
{
  int type;
  int i;
  
  if(argint(0, &type) < 0)
    return -1;
  
  if(type != SOCK_DGRAM && type != SOCK_STREAM && type != SOCK_RAW)
    return -1;
  
  // Find free socket
  for(i = 0; i < 16; i++) {
    if(!sockets[i].used) {
      sockets[i].used = 1;
      sockets[i].type = type;
      sockets[i].local_port = 0;
      sockets[i].remote_port = 0;
      sockets[i].rxq = 0;
      sockets[i].state = TCP_CLOSED;
      sockets[i].local_ip = htonl(0x0a000215); // 10.0.2.15
      return i;
    }
  }
  
  return -1;
}

int
sys_connect(void)
{
  int sockfd;
  uint ip;
  int port;
  
  if(argint(0, &sockfd) < 0 || argint(1, (int*)&ip) < 0 || argint(2, &port) < 0)
    return -1;
  
  if(sockfd < 0 || sockfd >= 16 || !sockets[sockfd].used)
    return -1;
  
  sockets[sockfd].remote_ip = ip;
  sockets[sockfd].remote_port = port;
  sockets[sockfd].local_port = 10000 + sockfd; // Simple port allocation
  
  // TCP connection
  if(sockets[sockfd].type == SOCK_STREAM) {
    extern uint tcp_seq;
    sockets[sockfd].iss = tcp_seq++;
    sockets[sockfd].snd_nxt = sockets[sockfd].iss;
    sockets[sockfd].snd_una = sockets[sockfd].iss;
    sockets[sockfd].state = TCP_SYN_SENT;
    tcp_send(&sockets[sockfd], TCP_SYN, 0, 0);
    
    // Wait for connection (simple busy wait - could be improved)
    int timeout = 1000000;
    while(sockets[sockfd].state != TCP_ESTABLISHED && timeout-- > 0);
    
    if(sockets[sockfd].state != TCP_ESTABLISHED)
      return -1;
  }
  
  return 0;
}

int
sys_send(void)
{
  int sockfd;
  char *buf;
  int len;
  struct mbuf *m;
  
  if(argint(0, &sockfd) < 0 || argptr(1, &buf, sizeof(buf)) < 0 || argint(2, &len) < 0)
    return -1;
  
  if(sockfd < 0 || sockfd >= 16 || !sockets[sockfd].used)
    return -1;
  
  if(len > 1500)
    return -1;
  
  if(sockets[sockfd].type == SOCK_DGRAM) {
    m = mbufalloc(0);
    if(!m)
      return -1;
    
    memmove(m->head, buf, len);
    m->len = len;
    net_tx_udp(sockets[sockfd].remote_ip, sockets[sockfd].local_port, 
               sockets[sockfd].remote_port, m);
    return len;
  } else if(sockets[sockfd].type == SOCK_STREAM) {
    if(sockets[sockfd].state != TCP_ESTABLISHED)
      return -1;
    
    tcp_send(&sockets[sockfd], TCP_ACK | TCP_PSH, buf, len);
    return len;
  } else if(sockets[sockfd].type == SOCK_RAW) {
    // For raw ICMP, expect buf to contain: [id(2)][seq(2)][data...]
    if(len < 4) return -1;
    
    ushort id = (buf[0] << 8) | buf[1];
    ushort seq = (buf[2] << 8) | buf[3];
    
    icmp_send(sockets[sockfd].remote_ip, id, seq, buf + 4, len - 4);
    return len;
  }
  
  return -1;
}

int
sys_recv(void)
{
  int sockfd;
  char *buf;
  int len;
  struct mbuf *m;
  int copylen;
  
  if(argint(0, &sockfd) < 0 || argptr(1, &buf, sizeof(buf)) < 0 || argint(2, &len) < 0)
    return -1;
  
  if(sockfd < 0 || sockfd >= 16 || !sockets[sockfd].used)
    return -1;
  
  // For UDP, wait for data to arrive (simple polling for now)
  if(sockets[sockfd].type == SOCK_DGRAM) {
    int timeout = 1000; // ~10 seconds
    while(timeout-- > 0 && !sockets[sockfd].rxq) {
      // No data yet, sleep briefly
      // In a real implementation, we'd use sleep/wakeup
      // For now, just yield the CPU
      int i;
      for(i = 0; i < 100000; i++); // Busy wait a bit
    }
  }
  
  m = sockets[sockfd].rxq;
  if(!m)
    return 0; // No data
  
  copylen = m->len < len ? m->len : len;
  memmove(buf, m->head, copylen);
  
  sockets[sockfd].rxq = 0;
  mbuffree(m);
  
  return copylen;
}

int
sys_close_socket(void)
{
  int sockfd;
  
  if(argint(0, &sockfd) < 0)
    return -1;
  
  if(sockfd < 0 || sockfd >= 16 || !sockets[sockfd].used)
    return -1;
  
  if(sockets[sockfd].rxq)
    mbuffree(sockets[sockfd].rxq);
  
  sockets[sockfd].used = 0;
  return 0;
}
