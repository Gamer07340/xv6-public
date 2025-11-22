// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);

static int panicked = 0;

static struct {
  struct spinlock lock;
    int locking;
    int rawmode;
} cons;

void
console_setmode(int mode)
{
  acquire(&cons.lock);
  cons.rawmode = mode;
  release(&cons.lock);
}

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory
static int ansi_state = 0;
static int ansi_val[2];
static int ansi_idx = 0;
static int ansi_attr = 0x0700; // black on white

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(ansi_state == 0){
    if(c == '\x1b'){
      ansi_state = 1;
    } else if(c == '\n'){
      pos += 80 - pos%80;
    } else if(c == BACKSPACE){
      if(pos > 0){
        --pos;
        crt[pos] = ' ' | ansi_attr;
      }
    } else if(c == '\b'){
      if(pos > 0) --pos;
    } else if(c == '\t'){
      pos += 8 - (pos % 8);
    } else {
      crt[pos++] = (c&0xff) | ansi_attr;
    }
  } else if(ansi_state == 1){
    if(c == '['){
      ansi_state = 2;
      ansi_val[0] = -1;
      ansi_val[1] = -1;
      ansi_idx = 0;
    } else {
      ansi_state = 0;
      // Emit the swallowed ESC? Too complex for now, just ignore invalid sequences
    }
  } else if(ansi_state == 2){
    if(c >= '0' && c <= '9'){
      if(ansi_val[ansi_idx] == -1) ansi_val[ansi_idx] = 0;
      ansi_val[ansi_idx] = ansi_val[ansi_idx] * 10 + (c - '0');
    } else if(c == ';'){
      if(ansi_idx < 1) ansi_idx++;
    } else {
      int v = ansi_val[0] == -1 ? 1 : ansi_val[0];
      switch(c){
      case 'A': pos -= 80 * v; break; // Up
      case 'B': pos += 80 * v; break; // Down
      case 'C': pos += v; break;      // Right
      case 'D': pos -= v; break;      // Left
      case 'H': // Cursor Position
        {
          int r = ansi_val[0] == -1 ? 0 : ansi_val[0]-1;
          int c = ansi_val[1] == -1 ? 0 : ansi_val[1]-1;
          if(r<0) r=0;
          if(r>24) r=24;
          if(c<0) c=0;
          if(c>79) c=79;
          pos = r * 80 + c;
        }
        break;
      case 'J': // Clear screen
        if(v == 2){
           int i;
           for(i = 0; i < 25*80; i++) crt[i] = ' ' | ansi_attr;
           pos = 0;
        }
        break;
      case 'm': // SGR - Select Graphic Rendition
        {
          int i;
          for(i=0; i<=ansi_idx; i++){
             int val = ansi_val[i];
             if(val == -1) val = 0;
             if(val == 0) ansi_attr = 0x0700; // Reset
             else if(val >= 30 && val <= 37) ansi_attr = (ansi_attr & 0xF000) | ((val-30)<<8); // FG
             else if(val >= 40 && val <= 47) ansi_attr = (ansi_attr & 0x0F00) | ((val-40)<<12); // BG
          }
        }
        break;
      }
      ansi_state = 0;
    }
  }

  if(pos < 0 || pos > 25*80)
    pos %= 25*80; // Wrap around safely instead of panic

  if(pos >= 25*80){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*24*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*80);
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  
  if((crt[pos] & 0xff) == 0)
    crt[pos] = ' ' | ansi_attr;
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    if(cons.rawmode){
      if(input.e-input.r < INPUT_BUF){
        input.buf[input.e++ % INPUT_BUF] = c;
        input.w = input.e;
        wakeup(&input.r);
      }
      continue;
    }

    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        consputc(c);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

int
consoleread(struct inode *ip, char *dst, int n, int off)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n, int off)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}

