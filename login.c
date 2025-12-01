#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "passwd.h"

int
main(int argc, char *argv[])
{
  char username[MAX_USERNAME];
  char password[MAX_PASSWORD];
  struct passwd *pw;

  while(1){
    printf(1, "login: ");
    gets(username, sizeof(username));
    if(username[strlen(username)-1] == '\n')
      username[strlen(username)-1] = 0;

    printf(1, "password: ");
    gets(password, sizeof(password));
    if(password[strlen(password)-1] == '\n')
      password[strlen(password)-1] = 0;
    if(password[strlen(password)-1] == '\r')
      password[strlen(password)-1] = 0;

    // Verify password
    if(verify_password(username, password) == 0){
      printf(1, "Login incorrect\n");
      continue;
    }

    // Look up user again to get details
    pw = getpwnam(username);
    if(pw == 0){
       printf(1, "Login incorrect\n"); // Should not happen if verify_password succeeded
       continue;
    }
    
    // Set UID and GID
    // Must set GID first while we still have root privileges!
    if(setgid(pw->gid) < 0 || setuid(pw->uid) < 0){
      printf(2, "login: failed to set uid/gid\n");
      continue;
    }
    
    // Change to home directory
    if(chdir(pw->homedir) < 0){
      printf(2, "login: warning: could not change to home directory\n");
    }
    
    // Execute shell
    char *argv[] = { pw->shell, 0 };
    exec(pw->shell, argv);
    printf(2, "login: exec %s failed\n", pw->shell);
    exit();
  }
}
