#include "types.h"
#include "defs.h"
#include "x86.h"
#include "traps.h"

// I/O Addresses of the two programmable interrupt controllers
#define IO_PIC1         0x20    // Master (IRQs 0-7)
#define IO_PIC2         0xA0    // Slave (IRQs 8-15)

// Don't use the 8259A interrupt controllers.  Xv6 assumes SMP hardware.
void
picinit(void)
{
  if(lapic){
    // mask all interrupts
    outb(IO_PIC1+1, 0xFF);
    outb(IO_PIC2+1, 0xFF);
    return;
  }

  // Initialize PIC
  outb(IO_PIC1, 0x11);
  outb(IO_PIC2, 0x11);
  outb(IO_PIC1+1, T_IRQ0);
  outb(IO_PIC2+1, T_IRQ0 + 8);
  outb(IO_PIC1+1, 4);
  outb(IO_PIC2+1, 2);
  outb(IO_PIC1+1, 1);
  outb(IO_PIC2+1, 1);

  // Mask all interrupts except timer (IRQ0) and keyboard (IRQ1)
  outb(IO_PIC1+1, 0xFC); 
  outb(IO_PIC2+1, 0xFF);
}

void
piceoi(void)
{
  outb(IO_PIC1, 0x20);
  // Send EOI to slave PIC if IRQ >= 8
  outb(IO_PIC2, 0x20);
}

void
picenable(int irq)
{
  if(lapic)
    return; // Use IOAPIC instead
  
  ushort port;
  uchar mask;
  
  if(irq < 8){
    port = IO_PIC1 + 1;
  } else {
    port = IO_PIC2 + 1;
    irq -= 8;
  }
  
  mask = inb(port);
  mask &= ~(1 << irq);
  outb(port, mask);
}

//PAGEBREAK!
// Blank page.
