#include "types.h"
#include "stat.h"
#include "user.h"

// Network byte order helpers
uint htonl(uint x);
ushort htons(ushort x);
uint ntohl(uint x);
ushort ntohs(ushort x);
uint dns_resolve(char *hostname);

// ICMP structures
struct icmp {
  uchar type;
  uchar code;
  ushort checksum;
  ushort id;
  ushort seq;
} __attribute__((packed));

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY 0

// Parse IP address from string (e.g., "8.8.8.8")
// Returns IP in network byte order
uint parse_ip(char *ip_str) {
  uint a = 0, b = 0, c = 0, d = 0;
  int i = 0, val = 0, part = 0;
  
  for (i = 0; ip_str[i]; i++) {
    if (ip_str[i] >= '0' && ip_str[i] <= '9') {
      val = val * 10 + (ip_str[i] - '0');
    } else if (ip_str[i] == '.') {
      if (part == 0) a = val;
      else if (part == 1) b = val;
      else if (part == 2) c = val;
      val = 0;
      part++;
    }
  }
  d = val;
  
  // Return in network byte order (big-endian)
  // For 10.0.2.2: a=10, b=0, c=2, d=2
  // Network byte order: 0x0a000202
  return (a << 24) | (b << 16) | (c << 8) | d;
}

// Check if string is an IP address
int is_ip_address(char *str) {
  int dots = 0;
  for (int i = 0; str[i]; i++) {
    if (str[i] == '.') dots++;
    else if (str[i] < '0' || str[i] > '9') return 0;
  }
  return dots == 3;
}

// Simple time measurement (count iterations)
uint get_ticks(void) {
  return uptime();
}

int
main(int argc, char *argv[])
{
  if(argc != 2){
    printf(2, "Usage: ping <hostname or IP>\n");
    printf(2, "Note: QEMU user networking may not forward external ICMP.\n");
    printf(2, "Try: ping 10.0.2.2 (gateway) for testing.\n");
    exit();
  }
  
  uint target_ip;
  char *target = argv[1];
  
  // Check if it's an IP address or hostname
  if (is_ip_address(target)) {
    target_ip = parse_ip(target);
    printf(1, "PING %s\n", target);
  } else {
    printf(1, "Resolving %s...\n", target);
    target_ip = dns_resolve(target);
    if (target_ip == 0) {
      printf(2, "ping: cannot resolve %s\n", target);
      exit();
    }
    uint ip_host = ntohl(target_ip);
    printf(1, "PING %s (%d.%d.%d.%d)\n", target,
           (ip_host >> 24) & 0xFF, (ip_host >> 16) & 0xFF,
           (ip_host >> 8) & 0xFF, ip_host & 0xFF);
  }
  
  printf(1, "Creating raw socket...\n");
  // Create raw socket for ICMP
  int sock = socket(3); // SOCK_RAW
  if (sock < 0) {
    printf(2, "ping: socket failed\n");
    exit();
  }
  
  // Set remote IP
  if (connect(sock, target_ip, 0) < 0) {
    printf(2, "ping: connect failed\n");
    close_socket(sock);
    exit();
  }
  
  // Send 4 ping requests
  int count = 4;
  int received = 0;
  ushort pid = getpid();
  
  for (int i = 0; i < count; i++) {
    // Prepare ICMP packet: [id(2)][seq(2)][data(56)]
    char packet[64];
    packet[0] = (pid >> 8) & 0xFF;
    packet[1] = pid & 0xFF;
    packet[2] = (i >> 8) & 0xFF;
    packet[3] = i & 0xFF;
    
    // Fill with pattern
    for (int j = 4; j < 64; j++) {
      packet[j] = 0x20 + (j % 64);
    }
    
    uint start_time = get_ticks();
    
    // Send ICMP echo request
    printf(1, "Sending ICMP request: id=%d seq=%d\n", pid, i);
    if (send(sock, packet, 64) < 0) {
      printf(2, "ping: send failed\n");
      continue;
    }
    
    // Wait for reply (with timeout)
    int timeout = 100; // ~1 second in ticks
    char reply[128];
    int n = 0;
    
    printf(1, "Waiting for reply...\n");
    while (timeout-- > 0) {
      n = recv(sock, reply, sizeof(reply));
      if (n > 0) {
        printf(1, "Received %d bytes\n", n);
        break;
      }
      sleep(1); // Sleep 10ms
    }
    
    uint end_time = get_ticks();
    uint rtt = (end_time - start_time) * 10; // Convert to ms
    
    if (n > 0) {
      // Parse ICMP reply
      struct icmp *icmp_reply = (struct icmp*)reply;
      ushort reply_id = ntohs(icmp_reply->id);
      ushort reply_seq = ntohs(icmp_reply->seq);
      
      if (reply_id == pid && reply_seq == i) {
        printf(1, "%d bytes from %s: icmp_seq=%d time=%d ms\n",
               n, target, i, rtt);
        received++;
      } else {
        printf(1, "Reply mismatch: id=%d seq=%d (expected id=%d seq=%d)\n",
               reply_id, reply_seq, pid, i);
      }
    } else {
      printf(1, "Request timeout for icmp_seq=%d\n", i);
    }
    
    // Wait between pings
    if (i < count - 1) {
      sleep(100); // 1 second
    }
  }
  
  // Print statistics
  printf(1, "\n--- %s ping statistics ---\n", target);
  printf(1, "%d packets transmitted, %d received, %d%% packet loss\n",
         count, received, ((count - received) * 100) / count);
  
  close_socket(sock);
  exit();
}
