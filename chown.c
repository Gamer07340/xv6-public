#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc < 4){
    printf(2, "Usage: chown <uid> <gid> <files...>\n");
    exit();
  }

  int uid = atoi(argv[1]);
  int gid = atoi(argv[2]);
  int i;

  for(i = 3; i < argc; i++){
    if(chown(argv[i], uid, gid) < 0){
      printf(2, "chown: failed to change ownership of %s\n", argv[i]);
    }
  }
  exit();
}
