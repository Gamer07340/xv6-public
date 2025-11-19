#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf(2, "Usage: mv old new\n");
    exit();
  }

  if(link(argv[1], argv[2]) < 0){
    printf(2, "mv: link failed\n");
    exit();
  }
  if(unlink(argv[1]) < 0){
    printf(2, "mv: unlink failed\n");
    exit();
  }
  exit();
}
