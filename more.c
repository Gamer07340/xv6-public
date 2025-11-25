#include "types.h"
#include "user.h"
#include "fcntl.h"

#define SCREEN_LINES 23 // Leave one line for "-- More --"

void
more(int fd_in, int fd_cmd)
{
  char buf[1];
  int lines = 0;
  int n;
  
  while((n = read(fd_in, buf, 1)) > 0){
    if(write(1, buf, 1) != 1) break; // Write to stdout
    
    if(buf[0] == '\n'){
      lines++;
    }
    
    if(lines >= SCREEN_LINES){
      printf(1, "-- More --");
      
      // Wait for input from console
      char c;
      while(read(fd_cmd, &c, 1) > 0){
         if(c == '\n' || c == '\r' || c == ' '){
             // Clear prompt
             printf(1, "\r          \r"); // Clear "-- More --"
             lines = 0;
             break;
         }
         if(c == 'q'){
             printf(1, "\n");
             exit();
         }
      }
    }
  }
}

int
main(int argc, char *argv[])
{
  int fd_cmd = open("/dev/console", O_RDONLY);
  if(fd_cmd < 0){
      printf(2, "more: cannot open console\n");
      exit();
  }

  if(argc <= 1){
    more(0, fd_cmd);
  } else {
    int i;
    for(i = 1; i < argc; i++){
      int fd = open(argv[i], O_RDONLY);
      if(fd < 0){
        printf(2, "more: cannot open %s\n", argv[i]);
        continue;
      }
      more(fd, fd_cmd);
      close(fd);
    }
  }
  close(fd_cmd);
  exit();
}
