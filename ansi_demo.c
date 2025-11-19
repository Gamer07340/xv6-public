#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  // Clear screen
  printf(1, "\x1b[2J");
  
  // Move cursor to 5,5
  printf(1, "\x1b[5;5H");
  
  // Red text
  printf(1, "\x1b[31mHello Red World!\x1b[0m\n");
  
  // Green text
  printf(1, "\x1b[32mHello Green World!\x1b[0m\n");
  
  // Blue background, White text
  printf(1, "\x1b[44;37m Blue Background \x1b[0m\n");
  
  // Move cursor around
  printf(1, "\x1b[10;10HMoving Cursor...");
  printf(1, "\x1b[12;10H...Down here");
  
  printf(1, "\n\n");
  exit();
}
