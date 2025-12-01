#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "passwd.h"
#include "sha256.h"

static struct passwd pw_entry;

// Parse a line from /etc/passwd
// Format: username:password:uid:gid:homedir:shell
static int
parseline(char *line, struct passwd *pw)
{
  char *p = line;
  int field = 0;
  int i;
  char tempbuf[32];  // Temporary buffer for UID/GID strings
  
  memset(pw, 0, sizeof(*pw));
  
  for(i = 0; *p && field < 6; p++) {
    if(*p == ':' || *p == '\n') {
      switch(field) {
        case 0: // username
          pw->username[i] = 0;
          break;
        case 1: // password
          pw->password[i] = 0;
          break;
        case 2: // uid
          tempbuf[i] = 0;
          pw->uid = atoi(tempbuf);
          break;
        case 3: // gid
          tempbuf[i] = 0;
          pw->gid = atoi(tempbuf);
          break;
        case 4: // homedir
          pw->homedir[i] = 0;
          break;
        case 5: // shell
          pw->shell[i] = 0;
          break;
      }
      field++;
      i = 0;
      if(*p == '\n') break;
    } else {
      switch(field) {
        case 0:
          if(i < MAX_USERNAME - 1) pw->username[i++] = *p;
          break;
        case 1:
          if(i < MAX_PASSWORD - 1) pw->password[i++] = *p;
          break;
        case 2:
        case 3:
          if(i < sizeof(tempbuf) - 1) tempbuf[i++] = *p;
          break;
        case 4:
          if(i < MAX_HOMEDIR - 1) pw->homedir[i++] = *p;
          break;
        case 5:
          if(i < MAX_SHELL - 1) pw->shell[i++] = *p;
          break;
      }
    }
  }
  
  return field >= 4; // At least username:password:uid:gid
}

// Look up user by username
struct passwd*
getpwnam(const char *username)
{
  int fd;
  char buf[256];
  int n, i;
  char line[256];
  int linelen = 0;
  
  fd = open(PASSWD_FILE, O_RDONLY);
  if(fd < 0)
    return 0;
  
  while((n = read(fd, buf, sizeof(buf))) > 0) {
    for(i = 0; i < n; i++) {
      if(buf[i] == '\n') {
        line[linelen] = '\n';
        line[linelen + 1] = 0;
        if(parseline(line, &pw_entry)) {
          if(strcmp(pw_entry.username, username) == 0) {
            close(fd);
            return &pw_entry;
          }
        }
        linelen = 0;
      } else if(linelen < sizeof(line) - 2) {
        line[linelen++] = buf[i];
      }
    }
  }
  
  close(fd);
  return 0;
}

// Look up user by UID
struct passwd*
getpwuid(int uid)
{
  int fd;
  char buf[256];
  int n, i;
  char line[256];
  int linelen = 0;
  
  fd = open(PASSWD_FILE, O_RDONLY);
  if(fd < 0)
    return 0;
  
  while((n = read(fd, buf, sizeof(buf))) > 0) {
    for(i = 0; i < n; i++) {
      if(buf[i] == '\n') {
        line[linelen] = '\n';
        line[linelen + 1] = 0;
        if(parseline(line, &pw_entry)) {
          if(pw_entry.uid == uid) {
            close(fd);
            return &pw_entry;
          }
        }
        linelen = 0;
      } else if(linelen < sizeof(line) - 2) {
        line[linelen++] = buf[i];
      }
    }
  }
  
  close(fd);
  return 0;
}

// Get next available UID (starting from 1000)
int
getnextuid(void)
{
  int fd;
  char buf[256];
  int n, i;
  char line[256];
  int linelen = 0;
  int maxuid = 999;
  
  fd = open(PASSWD_FILE, O_RDONLY);
  if(fd < 0)
    return 1000;
  
  while((n = read(fd, buf, sizeof(buf))) > 0) {
    for(i = 0; i < n; i++) {
      if(buf[i] == '\n') {
        line[linelen] = '\n';
        line[linelen + 1] = 0;
        if(parseline(line, &pw_entry)) {
          if(pw_entry.uid > maxuid && pw_entry.uid < 60000)
            maxuid = pw_entry.uid;
        }
        linelen = 0;
      } else if(linelen < sizeof(line) - 2) {
        line[linelen++] = buf[i];
      }
    }
  }
  
  close(fd);
  return maxuid + 1;
}

// Add a new user
int
adduser(const char *username, const char *password, int uid, int gid, const char *homedir, const char *shell)
{
  int fd;
  char line[256];
  
  // Check if user already exists
  if(getpwnam(username) != 0)
    return -1;
  
  // Open passwd file for append
  fd = open(PASSWD_FILE, O_WRONLY);
  if(fd < 0)
    return -1;
  
  // Seek to end
  lseek(fd, 0, 2);
  
  // Format: username:password:uid:gid:homedir:shell\n
  strcpy(line, username);
  strcat(line, ":");
  strcat(line, password);
  strcat(line, ":");
  
  // Convert uid to string
  char uidstr[16];
  int i = 0, tmp = uid;
  if(tmp == 0) {
    uidstr[i++] = '0';
  } else {
    char rev[16];
    int j = 0;
    while(tmp > 0) {
      rev[j++] = '0' + (tmp % 10);
      tmp /= 10;
    }
    while(j > 0) {
      uidstr[i++] = rev[--j];
    }
  }
  uidstr[i] = 0;
  strcat(line, uidstr);
  strcat(line, ":");
  
  // Convert gid to string
  char gidstr[16];
  i = 0;
  tmp = gid;
  if(tmp == 0) {
    gidstr[i++] = '0';
  } else {
    char rev[16];
    int j = 0;
    while(tmp > 0) {
      rev[j++] = '0' + (tmp % 10);
      tmp /= 10;
    }
    while(j > 0) {
      gidstr[i++] = rev[--j];
    }
  }
  gidstr[i] = 0;
  strcat(line, gidstr);
  strcat(line, ":");
  
  strcat(line, homedir);
  strcat(line, ":");
  strcat(line, shell);
  strcat(line, "\n");
  
  write(fd, line, strlen(line));
  close(fd);
  
  return 0;
}

// Delete a user
int
deluser(const char *username)
{
  int fd, tmpfd;
  char buf[256];
  int n, i;
  char line[256];
  int linelen = 0;
  
  fd = open(PASSWD_FILE, O_RDONLY);
  if(fd < 0)
    return -1;
  
  tmpfd = open("/etc/passwd.tmp", O_CREATE | O_WRONLY);
  if(tmpfd < 0) {
    close(fd);
    return -1;
  }
  
  while((n = read(fd, buf, sizeof(buf))) > 0) {
    for(i = 0; i < n; i++) {
      if(buf[i] == '\n') {
        line[linelen] = '\n';
        line[linelen + 1] = 0;
        if(parseline(line, &pw_entry)) {
          if(strcmp(pw_entry.username, username) != 0) {
            write(tmpfd, line, linelen + 1);
          }
        }
        linelen = 0;
      } else if(linelen < sizeof(line) - 2) {
        line[linelen++] = buf[i];
      }
    }
  }
  
  close(fd);
  close(tmpfd);
  
  unlink(PASSWD_FILE);
  link("/etc/passwd.tmp", PASSWD_FILE);
  unlink("/etc/passwd.tmp");
  
  return 0;
}

// Change user password
int
setpasswd(const char *username, const char *password)
{
  int fd, tmpfd;
  char buf[256];
  int n, i;
  char line[256];
  int linelen = 0;
  
  fd = open(PASSWD_FILE, O_RDONLY);
  if(fd < 0)
    return -1;
  
  tmpfd = open("/etc/passwd.tmp", O_CREATE | O_WRONLY);
  if(tmpfd < 0) {
    close(fd);
    return -1;
  }
  
  while((n = read(fd, buf, sizeof(buf))) > 0) {
    for(i = 0; i < n; i++) {
      if(buf[i] == '\n') {
        line[linelen] = '\n';
        line[linelen + 1] = 0;
        if(parseline(line, &pw_entry)) {
          if(strcmp(pw_entry.username, username) == 0) {
            // Write updated entry
            char newline[256];
            strcpy(newline, username);
            strcat(newline, ":");
            strcat(newline, password);
            strcat(newline, ":");
            
            // Add uid
            char uidstr[16];
            int j = 0, tmp = pw_entry.uid;
            if(tmp == 0) {
              uidstr[j++] = '0';
            } else {
              char rev[16];
              int k = 0;
              while(tmp > 0) {
                rev[k++] = '0' + (tmp % 10);
                tmp /= 10;
              }
              while(k > 0) {
                uidstr[j++] = rev[--k];
              }
            }
            uidstr[j] = 0;
            strcat(newline, uidstr);
            strcat(newline, ":");
            
            // Add gid
            char gidstr[16];
            j = 0;
            tmp = pw_entry.gid;
            if(tmp == 0) {
              gidstr[j++] = '0';
            } else {
              char rev[16];
              int k = 0;
              while(tmp > 0) {
                rev[k++] = '0' + (tmp % 10);
                tmp /= 10;
              }
              while(k > 0) {
                gidstr[j++] = rev[--k];
              }
            }
            gidstr[j] = 0;
            strcat(newline, gidstr);
            strcat(newline, ":");
            
            strcat(newline, pw_entry.homedir);
            strcat(newline, ":");
            strcat(newline, pw_entry.shell);
            strcat(newline, "\n");
            
            write(tmpfd, newline, strlen(newline));
          } else {
            write(tmpfd, line, linelen + 1);
          }
        }
        linelen = 0;
      } else if(linelen < sizeof(line) - 2) {
        line[linelen++] = buf[i];
      }
    }
  }
  
  close(fd);
  close(tmpfd);
  
  unlink(PASSWD_FILE);
  link("/etc/passwd.tmp", PASSWD_FILE);
  unlink("/etc/passwd.tmp");
  
  return 0;
}

// Verify password for a user
// Returns 1 if password is correct, 0 otherwise
int
verify_password(const char *username, const char *password)
{
  struct passwd *pw = getpwnam(username);
  if(pw == 0)
    return 0;
  
  // Hash the provided password with SHA256
  unsigned char hash[32];
  char hex_hash[65];
  
  // Hash the password
  sha256_hash((const unsigned char*)password, strlen(password), hash);
  sha256_to_hex(hash, hex_hash);
  
  // Compare with stored hash
  return strcmp(hex_hash, pw->password) == 0;
}
