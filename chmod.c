#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc < 3){
    printf(2, "Usage: chmod <mode> <files...>\n");
    exit();
  }

  // Simple octal parser
  int octal_mode = 0;
  char *p = argv[1];
  while(*p){
    if(*p < '0' || *p > '7'){
      printf(2, "chmod: invalid mode\n");
      exit();
    }
    octal_mode = octal_mode * 8 + (*p - '0');
    p++;
  }

  int i;
  for(i = 2; i < argc; i++){
    if(chmod(argv[i], octal_mode) < 0){
      printf(2, "chmod: failed to change mode of %s\n", argv[i]);
    }
  }
  exit();
}
