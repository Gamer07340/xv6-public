#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "passwd.h"
#include "sha256.h"

int
main(int argc, char *argv[])
{
  char *username;
  char password[MAX_PASSWORD];
  int uid, gid;
  char homedir[MAX_HOMEDIR];
  char shell[MAX_SHELL];
  
  if(argc < 2){
    printf(2, "Usage: useradd <username> [uid] [gid]\n");
    exit();
  }
  
  username = argv[1];
  
  // Check if running as root
  if(getuid() != 0){
    printf(2, "useradd: permission denied (must be root)\n");
    exit();
  }
  
  // Check if user already exists
  if(getpwnam(username) != 0){
    printf(2, "useradd: user '%s' already exists\n", username);
    exit();
  }
  
  // Get UID (auto-assign if not specified)
  if(argc >= 3){
    uid = atoi(argv[2]);
  } else {
    uid = getnextuid();
  }
  
  // Get GID (default to same as UID)
  if(argc >= 4){
    gid = atoi(argv[3]);
  } else {
    gid = uid;
  }
  
  // Prompt for password
  printf(1, "Enter password for %s: ", username);
  gets(password, sizeof(password));
  if(password[strlen(password)-1] == '\n')
    password[strlen(password)-1] = 0;
  if(password[strlen(password)-1] == '\r')
    password[strlen(password)-1] = 0;
  
  // Hash password
  unsigned char hash[32];
  char hex_hash[65];
  sha256_hash((const unsigned char*)password, strlen(password), hash);
  sha256_to_hex(hash, hex_hash);

  // Set home directory
  strcpy(homedir, "/home/");
  strcat(homedir, username);
  
  // Set default shell
  strcpy(shell, "/bin/sh");
  
  // Add user to database
  if(adduser(username, hex_hash, uid, gid, homedir, shell) < 0){
    printf(2, "useradd: failed to add user\n");
    exit();
  }
  
  // Create home directory
  if(mkdir(homedir) < 0){
    printf(2, "useradd: warning: failed to create home directory %s\n", homedir);
  } else {
    // Change ownership of home directory
    if(chown(homedir, uid, gid) < 0){
      printf(2, "useradd: warning: failed to set ownership of %s\n", homedir);
    }
    // Set permissions to 755
    if(chmod(homedir, 0755) < 0){
      printf(2, "useradd: warning: failed to set permissions on %s\n", homedir);
    }
  }
  
  printf(1, "User '%s' created successfully (UID=%d, GID=%d)\n", username, uid, gid);
  exit();
}
