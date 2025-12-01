#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "passwd.h"

int
main(int argc, char *argv[])
{
  char *username;
  
  if(argc < 2){
    printf(2, "Usage: userdel <username>\n");
    exit();
  }
  
  username = argv[1];
  
  // Check if running as root
  if(getuid() != 0){
    printf(2, "userdel: permission denied (must be root)\n");
    exit();
  }
  
  // Check if user exists
  struct passwd *pw = getpwnam(username);
  if(pw == 0){
    printf(2, "userdel: user '%s' does not exist\n", username);
    exit();
  }
  
  // Don't allow deleting root
  if(pw->uid == 0){
    printf(2, "userdel: cannot delete root user\n");
    exit();
  }
  
  // Delete user from database
  if(deluser(username) < 0){
    printf(2, "userdel: failed to delete user\n");
    exit();
  }
  
  printf(1, "User '%s' deleted successfully\n", username);
  exit();
}
