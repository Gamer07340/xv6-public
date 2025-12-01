#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "passwd.h"

int
main(int argc, char *argv[])
{
  char *username;
  char password[MAX_PASSWORD];
  struct passwd *pw;
  
  // Default to root if no argument
  if(argc > 1){
    username = argv[1];
  } else {
    username = "root";
  }
  
  // Look up target user
  pw = getpwnam(username);
  if(pw == 0){
    printf(2, "su: user '%s' not found\n", username);
    exit();
  }
  
  // If not root, require password
  if(getuid() != 0){
    printf(1, "Password: ");
    gets(password, sizeof(password));
    if(password[strlen(password)-1] == '\n')
      password[strlen(password)-1] = 0;
    if(password[strlen(password)-1] == '\r')
      password[strlen(password)-1] = 0;
    
    if(verify_password(username, password) == 0){
      printf(2, "su: authentication failure\n");
      exit();
    }
  }
  
  // Set UID and GID
  // Must set GID first while we still have root privileges!
  if(setgid(pw->gid) < 0 || setuid(pw->uid) < 0){
    printf(2, "su: failed to set uid/gid\n");
    exit();
  }
  
  // Change to home directory
  if(chdir(pw->homedir) < 0){
    printf(2, "su: warning: could not change to home directory\n");
  }
  
  // Execute shell
  char *argv_sh[] = { pw->shell, 0 };
  exec(pw->shell, argv_sh);
  printf(2, "su: exec %s failed\n", pw->shell);
  exit();
}
