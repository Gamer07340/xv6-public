// DNS helper functions for user programs
#include "types.h"
#include "user.h"

// Forward declarations
uint htonl(uint x);
ushort htons(ushort x);
uint ntohl(uint x);
ushort ntohs(ushort x);

// DNS query structure helpers
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

// Convert hostname to DNS query format
// e.g., "google.com" -> "\06google\03com\00"
int hostname_to_dns(char *hostname, char *dns_name) {
  int i = 0, j = 0, len_pos = 0;
  
  dns_name[j++] = 0; // Length placeholder
  len_pos = 0;
  
  for (i = 0; hostname[i]; i++) {
    if (hostname[i] == '.') {
      dns_name[len_pos] = j - len_pos - 1;
      dns_name[j++] = 0;
      len_pos = j - 1;
    } else {
      dns_name[j++] = hostname[i];
    }
  }
  dns_name[len_pos] = j - len_pos - 1;
  dns_name[j++] = 0;
  
  return j;
}

// Resolve hostname to IP address using DNS
// Returns IP in network byte order, or 0 on failure
uint dns_resolve(char *hostname) {
  int sock = socket(1); // SOCK_DGRAM
  if (sock < 0) {
    printf(2, "dns_resolve: socket failed\n");
    return 0;
  }
  
  // Connect to DNS server
  // Try QEMU's built-in DNS server first (10.0.2.3)
  // If that fails, fall back to 8.8.8.8
  uint dns_server = (10 << 24) | (0 << 16) | (2 << 8) | 3; // 10.0.2.3
  
  printf(2, "DNS: Trying QEMU DNS server 10.0.2.3...\n");
  if (connect(sock, dns_server, 53) < 0) {
    printf(2, "dns_resolve: connect to 10.0.2.3 failed, trying 8.8.8.8\n");
    dns_server = (8 << 24) | (8 << 16) | (8 << 8) | 8; // 8.8.8.8
    if (connect(sock, dns_server, 53) < 0) {
      printf(2, "dns_resolve: connect failed\n");
      close_socket(sock);
      return 0;
    }
  }
  
  // Build DNS query
  char query[512];
  struct dns_header *hdr = (struct dns_header*)query;
  
  hdr->id = htons(1234);
  hdr->flags = htons(0x0100); // Standard query, recursion desired
  hdr->qdcount = htons(1);
  hdr->ancount = 0;
  hdr->nscount = 0;
  hdr->arcount = 0;
  
  int qname_len = hostname_to_dns(hostname, query + sizeof(struct dns_header));
  
  // Add QTYPE and QCLASS
  char *qtype = query + sizeof(struct dns_header) + qname_len;
  qtype[0] = 0; qtype[1] = DNS_TYPE_A;    // Type A
  qtype[2] = 0; qtype[3] = DNS_CLASS_IN;  // Class IN
  
  int query_len = sizeof(struct dns_header) + qname_len + 4;
  
  // Send query
  printf(2, "DNS: Sending query to 8.8.8.8, len=%d\n", query_len);
  if (send(sock, query, query_len) < 0) {
    printf(2, "dns_resolve: send failed\n");
    close_socket(sock);
    return 0;
  }
  
  // Receive response (with retries)
  char response[512];
  int n = 0;
  int retries = 10;
  
  printf(2, "DNS: Waiting for response...\n");
  while (retries-- > 0 && n == 0) {
    n = recv(sock, response, sizeof(response));
    if (n == 0) {
      sleep(10); // Wait 100ms
    }
  }
  
  printf(2, "DNS: Received %d bytes\n", n);
  
  if (n < sizeof(struct dns_header)) {
    printf(2, "dns_resolve: recv failed or too short (got %d bytes)\n", n);
    close_socket(sock);
    return 0;
  }
  
  struct dns_header *resp_hdr = (struct dns_header*)response;
  if (ntohs(resp_hdr->ancount) == 0) {
    printf(2, "dns_resolve: no answers\n");
    close_socket(sock);
    return 0;
  }
  
  // Parse response - skip question section
  char *ptr = response + sizeof(struct dns_header);
  while (*ptr) {
    if ((*ptr & 0xC0) == 0xC0) {
      ptr += 2;
      break;
    }
    ptr += *ptr + 1;
  }
  ptr += 4; // Skip QTYPE and QCLASS
  
  // Parse answer section
  // Skip name (usually compressed pointer)
  if ((*ptr & 0xC0) == 0xC0) {
    ptr += 2;
  } else {
    while (*ptr) ptr += *ptr + 1;
    ptr++;
  }
  
  ushort type = (ptr[0] << 8) | ptr[1];
  ptr += 8; // Skip TYPE, CLASS, TTL
  ushort rdlength = (ptr[0] << 8) | ptr[1];
  ptr += 2;
  
  if (type == DNS_TYPE_A && rdlength == 4) {
    uint ip = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
    close_socket(sock);
    return htonl(ip);
  }
  
  close_socket(sock);
  return 0;
}

// Helper functions for network byte order
// Network byte order is big-endian
// x86 is little-endian, so we need to swap bytes
uint htonl(uint x) {
  return ((x & 0xFF000000) >> 24) | 
         ((x & 0x00FF0000) >> 8) | 
         ((x & 0x0000FF00) << 8) | 
         ((x & 0x000000FF) << 24);
}

ushort htons(ushort x) {
  return ((x & 0xFF00) >> 8) | ((x & 0x00FF) << 8);
}

uint ntohl(uint x) {
  return htonl(x); // Same operation
}

ushort ntohs(ushort x) {
  return htons(x); // Same operation
}
