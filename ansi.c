#include "types.h"
#include "user.h"
#include "ansi.h"

void
cursor(int x, int y)
{
  printf(1, "\x1b[%d;%dH", y + 1, x + 1);
}

void
color(int fg, int bg)
{
  // Map 0-7 to 30-37 (fg) and 40-47 (bg)
  // If fg or bg is -1 (or out of range), we could ignore or reset.
  // For now, let's assume valid inputs 0-7 or use a reset if needed.
  // But the request is simple color(fg, bg).
  // Let's just do the math.
  
  int fg_code = 30 + fg;
  int bg_code = 40 + bg;
  
  printf(1, "\x1b[%d;%dm", fg_code, bg_code);
}

void
setchar(int x, int y, char c)
{
  cursor(x, y);
  printf(1, "%c", c);
}

void
putstr(char *s)
{
  printf(1, "%s", s);
}

void
reset(void)
{
  printf(1, "\x1b[2J");
}
