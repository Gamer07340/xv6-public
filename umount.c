#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    printf(1, "Usage: umount dir\n");
    exit();
  }
  if(umount(argv[1]) < 0){
    printf(1, "umount failed\n");
  }
  exit();
}
