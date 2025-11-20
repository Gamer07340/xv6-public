#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main()
{
  char buf[512];
  int fd, i, sectors;

  fd = open("big.file", O_CREATE | O_WRONLY);
  if(fd < 0){
    printf(1, "bigfile: cannot open big.file for writing\n");
    exit();
  }

  sectors = 0;
  while(sectors < 300){ // 300 sectors = 150KB
    *(int*)buf = sectors;
    int cc = write(fd, buf, sizeof(buf));
    if(cc <= 0){
      printf(1, "bigfile: write error at sector %d\n", sectors);
      exit();
    }
    sectors++;
  }

  close(fd);

  fd = open("big.file", O_RDONLY);
  if(fd < 0){
    printf(1, "bigfile: cannot open big.file for reading\n");
    exit();
  }

  for(i = 0; i < 300; i++){
    int cc = read(fd, buf, sizeof(buf));
    if(cc <= 0){
      printf(1, "bigfile: read error at sector %d\n", i);
      exit();
    }
    if(*(int*)buf != i){
      printf(1, "bigfile: read the wrong data (%d) for sector %d\n",
             *(int*)buf, i);
      exit();
    }
  }

  printf(1, "bigfile test passed\n");
  exit();
}
