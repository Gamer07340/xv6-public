#ifndef _PASSWD_H_
#define _PASSWD_H_

#define MAX_USERNAME 32
#define MAX_PASSWORD 65  // SHA256 hex string is 64 chars + null terminator
#define MAX_HOMEDIR 64
#define MAX_SHELL 32
#define PASSWD_FILE "/etc/passwd"

// User database entry
struct passwd {
  char username[MAX_USERNAME];
  char password[MAX_PASSWORD];
  int uid;
  int gid;
  char homedir[MAX_HOMEDIR];
  char shell[MAX_SHELL];
};

// User database functions
struct passwd* getpwnam(const char *username);
struct passwd* getpwuid(int uid);
int adduser(const char *username, const char *password, int uid, int gid, const char *homedir, const char *shell);
int deluser(const char *username);
int setpasswd(const char *username, const char *password);
int getnextuid(void);
int verify_password(const char *username, const char *password);

#endif
