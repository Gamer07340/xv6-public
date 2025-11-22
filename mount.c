#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf(1, "Usage: mount device dir\n");
    exit();
  }
  if(mount(argv[1], argv[2]) < 0){
    printf(1, "mount failed\n");
  }
  exit();
}
