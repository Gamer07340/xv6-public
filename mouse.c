#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

#define MOUSE_IRQ   12

struct {
  struct spinlock lock;
  uchar buf[256];
  uint r, w;
  struct proc *waiting;
} mouseq;

void
mousewait(int is_read)
{
  int timeout = 100000;
  while(timeout--){
    if(is_read){
      if((inb(PS2_STATUS) & 1) == 1) return;
    } else {
      if((inb(PS2_STATUS) & 2) == 0) return;
    }
  }
}

void
mousecmd(uchar cmd)
{
  mousewait(0);
  outb(PS2_CMD, 0xD4);
  mousewait(0);
  outb(PS2_DATA, cmd);
}

void
mouse_init(void)
{
  initlock(&mouseq.lock, "mouse");

  mousewait(0);
  outb(PS2_CMD, 0xA8); // Enable mouse interface

  mousewait(0);
  outb(PS2_CMD, 0x20); // Read config
  mousewait(1);
  uchar status = inb(PS2_DATA);
  status |= 2;
  mousewait(0);
  outb(PS2_CMD, 0x60); // Write config
  mousewait(0);
  outb(PS2_DATA, status);

  mousecmd(0xF6); // Set defaults
  mousecmd(0xF4); // Enable data reporting

  if(lapic)
    ioapicenable(MOUSE_IRQ, 0);
  else
    picenable(MOUSE_IRQ);

  devsw[MOUSE].read = mouseread;
}

void
mouseintr(void)
{
  uchar status = inb(PS2_STATUS);
  if((status & 1) == 0 || (status & 0x20) == 0)
    return;

  uchar data = inb(PS2_DATA);

  acquire(&mouseq.lock);
  if(mouseq.w - mouseq.r < sizeof(mouseq.buf)){
    mouseq.buf[mouseq.w++ % sizeof(mouseq.buf)] = data;
    wakeup(&mouseq.r);
  }
  release(&mouseq.lock);
}

int
mouseread(struct inode *ip, char *dst, int n, int off)
{
  int target = n;
  acquire(&mouseq.lock);
  while(n > 0){
    while(mouseq.r == mouseq.w){
      if(myproc()->killed){
        release(&mouseq.lock);
        return -1;
      }
      sleep(&mouseq.r, &mouseq.lock);
    }
    *dst++ = mouseq.buf[mouseq.r++ % sizeof(mouseq.buf)];
    n--;
  }
  release(&mouseq.lock);
  return target - n;
}
