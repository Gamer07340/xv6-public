#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

void
test_perm(void)
{
  printf(1, "Starting permission test...\n");
  printf(1, "Current UID: %d\n", getuid());

  // Create a file as root
  int fd = open("testfile", O_CREATE|O_RDWR);
  if(fd < 0){
    printf(1, "Failed to create testfile\n");
    exit();
  }
  write(fd, "hello", 5);
  close(fd);

  // Change owner to 1000:1000
  if(chown("testfile", 1000, 1000) < 0){
    printf(1, "chown failed\n");
    exit();
  }

  // Change mode to 600 (owner read/write only)
  // 600 octal = 384 decimal
  if(chmod("testfile", 384) < 0){
    printf(1, "chmod failed\n");
    exit();
  }

  // Switch to user 1001
  printf(1, "Switching to user 1001...\n");
  if(setuid(1001) < 0 || setgid(1001) < 0){
    printf(1, "setuid/gid failed\n");
    exit();
  }

  // Try to open file (should fail)
  fd = open("testfile", O_RDONLY);
  if(fd >= 0){
    printf(1, "ERROR: User 1001 could open file owned by 1000 with mode 600\n");
    close(fd);
  } else {
    printf(1, "SUCCESS: User 1001 denied access to file owned by 1000\n");
  }

  // Switch to user 1000 (should fail because we are not root anymore)
  if(setuid(1000) >= 0){
     printf(1, "ERROR: User 1001 could setuid to 1000\n");
  } else {
     printf(1, "SUCCESS: User 1001 denied setuid\n");
  }

  exit();
}

int
main(int argc, char *argv[])
{
  test_perm();
  exit();
}
