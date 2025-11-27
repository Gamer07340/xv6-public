#include "types.h"
#include "defs.h"
#include "traps.h"
#include "x86.h"

#define IO_TIMER1       0x40
#define TIMER_MODE      0x43
#define TIMER_SEL0      0x00    // select counter 0
#define TIMER_RATEGEN   0x04    // mode 2, rate generator
#define TIMER_16BIT     0x30    // r/w counter 16 bits, LSB first
#define TIMER_DIV(x)    ((1193182+(x)/2)/(x))

void
timerinit(void)
{
  // Interrupt 100 times/sec.
  outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
  outb(IO_TIMER1, TIMER_DIV(100) % 256);
  outb(IO_TIMER1, TIMER_DIV(100) / 256);
}
