#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  char buf[512];
  
  if(getcwd(buf, sizeof(buf)) < 0){
    printf(2, "pwd: failed to get current directory\n");
    exit();
  }
  
  printf(1, "%s\n", buf);
  exit();
}
