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
  char oldpass[MAX_PASSWORD];
  char newpass[MAX_PASSWORD];
  char confirm[MAX_PASSWORD];
  
  if(argc < 2){
    // Change current user's password
    struct passwd *pw = getpwuid(getuid());
    if(pw == 0){
      printf(2, "passwd: cannot find current user\n");
      exit();
    }
    username = pw->username;
  } else {
    username = argv[1];
    
    // Only root can change other users' passwords
    if(getuid() != 0){
      printf(2, "passwd: permission denied\n");
      exit();
    }
  }
  
  // Check if user exists
  struct passwd *pw = getpwnam(username);
  if(pw == 0){
    printf(2, "passwd: user '%s' does not exist\n", username);
    exit();
  }
  
  // If not root, verify old password
  if(getuid() != 0){
    printf(1, "Enter old password: ");
    gets(oldpass, sizeof(oldpass));
    if(oldpass[strlen(oldpass)-1] == '\n')
      oldpass[strlen(oldpass)-1] = 0;
    
    if(verify_password(username, oldpass) == 0){
      printf(2, "passwd: incorrect password\n");
      exit();
    }
  }
  
  // Get new password
  printf(1, "Enter new password: ");
  gets(newpass, sizeof(newpass));
  if(newpass[strlen(newpass)-1] == '\n')
    newpass[strlen(newpass)-1] = 0;
  
  // Confirm new password
  printf(1, "Confirm new password: ");
  gets(confirm, sizeof(confirm));
  if(confirm[strlen(confirm)-1] == '\n')
    confirm[strlen(confirm)-1] = 0;
  
  if(strcmp(newpass, confirm) != 0){
    printf(2, "passwd: passwords do not match\n");
    exit();
  }
  
  // Hash new password
  unsigned char hash[32];
  char hex_hash[65];
  sha256_hash((const unsigned char*)newpass, strlen(newpass), hash);
  sha256_to_hex(hash, hex_hash);

  // Update password
  if(setpasswd(username, hex_hash) < 0){
    printf(2, "passwd: failed to update password\n");
    exit();
  }
  
  printf(1, "Password updated successfully\n");
  exit();
}
