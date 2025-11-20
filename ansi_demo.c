#include "types.h"
#include "user.h"
#include "ansi.h"

int
main(int argc, char *argv[])
{
  // Clear screen (manually for now as not in lib, or use cursor/putstr)
  reset();
  // Move cursor to 5,5
  cursor(5, 5);
  
  // Red text
  color(ANSI_RED, ANSI_BLACK);
  putstr("Hello Red World!\n");
  
  // Green text
  color(ANSI_GREEN, ANSI_BLACK);
  putstr("Hello Green World!\n");
  
  // Blue background, White text
  color(ANSI_WHITE, ANSI_BLUE);
  putstr(" Blue Background ");
  
  // Reset color
  printf(1, "\x1b[0m\n"); // Resetting manually or I should add a reset? 
                          // The user didn't ask for reset, but it's good practice.
                          // I'll just leave the color as is or set it to white/black.
  color(ANSI_WHITE, ANSI_BLACK);

  // Move cursor around
  cursor(10, 10);
  putstr("Moving Cursor...");
  cursor(10, 12); // Note: original was 12;10 (y;x) -> x=10, y=12
  putstr("...Down here");
  
  // setchar demo
  setchar(15, 15, 'X');
  
  printf(1, "\n\n");
  exit();
}
