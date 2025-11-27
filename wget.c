#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

uint htonl(uint x);
ushort htons(ushort x);
uint ntohl(uint x);
ushort ntohs(ushort x);
uint dns_resolve(char *hostname);

// Parse URL into hostname and path
void parse_url(char *url, char *hostname, char *path) {
  int i = 0, j = 0;
  
  // Skip http://
  if (url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p' &&
      url[4] == ':' && url[5] == '/' && url[6] == '/') {
    i = 7;
  }
  
  // Extract hostname
  while (url[i] && url[i] != '/' && url[i] != ':') {
    hostname[j++] = url[i++];
  }
  hostname[j] = 0;
  
  // Skip port if present
  if (url[i] == ':') {
    while (url[i] && url[i] != '/') i++;
  }
  
  // Extract path
  j = 0;
  if (url[i] == '/') {
    while (url[i]) {
      path[j++] = url[i++];
    }
  } else {
    path[j++] = '/';
  }
  path[j] = 0;
}

// Extract filename from path
void get_filename(char *path, char *filename) {
  int i = 0, last_slash = -1;
  
  // Find last slash
  for (i = 0; path[i]; i++) {
    if (path[i] == '/') last_slash = i;
  }
  
  // Copy everything after last slash
  i = 0;
  if (last_slash >= 0) {
    int j = last_slash + 1;
    while (path[j]) {
      filename[i++] = path[j++];
    }
  }
  
  // If empty or just "/", use "index.html"
  if (i == 0) {
    filename[0] = 'i'; filename[1] = 'n'; filename[2] = 'd';
    filename[3] = 'e'; filename[4] = 'x'; filename[5] = '.';
    filename[6] = 'h'; filename[7] = 't'; filename[8] = 'm';
    filename[9] = 'l';
    i = 10;
  }
  filename[i] = 0;
}

// Find start of HTTP body (after headers)
char* find_body(char *response, int len) {
  int i;
  for (i = 0; i < len - 3; i++) {
    if (response[i] == '\r' && response[i+1] == '\n' &&
        response[i+2] == '\r' && response[i+3] == '\n') {
      return response + i + 4;
    }
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  if(argc != 2){
    printf(2, "Usage: wget <url>\n");
    printf(2, "Example: wget http://example.com/index.html\n");
    exit();
  }
  
  char hostname[256];
  char path[256];
  char filename[256];
  
  parse_url(argv[1], hostname, path);
  get_filename(path, filename);
  
  printf(1, "URL: %s\n", argv[1]);
  printf(1, "Host: %s\n", hostname);
  printf(1, "Path: %s\n", path);
  printf(1, "Saving to: %s\n", filename);
  
  // Resolve hostname
  printf(1, "Resolving %s...\n", hostname);
  uint ip = dns_resolve(hostname);
  if (ip == 0) {
    printf(2, "wget: cannot resolve %s\n", hostname);
    exit();
  }
  
  uint ip_host = ntohl(ip);
  printf(1, "Resolved to %d.%d.%d.%d\n",
         (ip_host >> 24) & 0xFF, (ip_host >> 16) & 0xFF,
         (ip_host >> 8) & 0xFF, ip_host & 0xFF);
  
  // Create TCP socket
  int sock = socket(2); // SOCK_STREAM
  if (sock < 0) {
    printf(2, "wget: socket failed\n");
    exit();
  }
  
  // Connect to server on port 80
  printf(1, "Connecting...\n");
  if (connect(sock, ip, 80) < 0) {
    printf(2, "wget: connect failed\n");
    close_socket(sock);
    exit();
  }
  
  printf(1, "Connected!\n");
  
  // Build HTTP GET request
  char request[512];
  int len = 0;
  
  // GET /path HTTP/1.0\r\n
  char *get = "GET ";
  for (int i = 0; get[i]; i++) request[len++] = get[i];
  for (int i = 0; path[i]; i++) request[len++] = path[i];
  char *http = " HTTP/1.0\r\n";
  for (int i = 0; http[i]; i++) request[len++] = http[i];
  
  // Host: hostname\r\n
  char *host_hdr = "Host: ";
  for (int i = 0; host_hdr[i]; i++) request[len++] = host_hdr[i];
  for (int i = 0; hostname[i]; i++) request[len++] = hostname[i];
  request[len++] = '\r';
  request[len++] = '\n';
  
  // Connection: close\r\n\r\n
  char *conn = "Connection: close\r\n\r\n";
  for (int i = 0; conn[i]; i++) request[len++] = conn[i];
  
  // Send request
  printf(1, "Sending HTTP request...\n");
  if (send(sock, request, len) < 0) {
    printf(2, "wget: send failed\n");
    close_socket(sock);
    exit();
  }
  
  // Receive response
  printf(1, "Receiving response...\n");
  char response[4096];
  int total = 0;
  int n;
  
  while ((n = recv(sock, response + total, sizeof(response) - total - 1)) > 0) {
    total += n;
    if (total >= sizeof(response) - 1) break;
  }
  
  response[total] = 0;
  close_socket(sock);
  
  if (total == 0) {
    printf(2, "wget: no response received\n");
    exit();
  }
  
  printf(1, "Received %d bytes\n", total);
  
  // Find body
  char *body = find_body(response, total);
  if (!body) {
    printf(2, "wget: could not find HTTP body\n");
    printf(1, "Response:\n%s\n", response);
    exit();
  }
  
  int body_len = total - (body - response);
  printf(1, "Body length: %d bytes\n", body_len);
  
  // Save to file
  int fd = open(filename, O_CREATE | O_WRONLY);
  if (fd < 0) {
    printf(2, "wget: cannot create file %s\n", filename);
    exit();
  }
  
  int written = write(fd, body, body_len);
  close(fd);
  
  if (written < 0) {
    printf(2, "wget: write failed\n");
    exit();
  }
  
  printf(1, "Saved %d bytes to %s\n", written, filename);
  printf(1, "Download complete!\n");
  
  exit();
}
