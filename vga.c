#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"

#define VGA_MISC_WRITE 0x3C2
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA  0x3D5
#define VGA_SEQ_INDEX  0x3C4
#define VGA_SEQ_DATA   0x3C5
#define VGA_GC_INDEX   0x3CE
#define VGA_GC_DATA    0x3CF
#define VGA_AC_INDEX   0x3C0
#define VGA_AC_WRITE   0x3C0
#define VGA_AC_READ    0x3C1

void
vga_write_regs(uchar *regs)
{
  int i;

  // MISC
  outb(VGA_MISC_WRITE, *regs);
  regs++;

  // SEQ
  for(i = 0; i < 5; i++){
    outb(VGA_SEQ_INDEX, i);
    outb(VGA_SEQ_DATA, *regs);
    regs++;
  }

  // CRTC
  outb(VGA_CRTC_INDEX, 0x03);
  uchar val = inb(VGA_CRTC_DATA);
  outb(VGA_CRTC_DATA, val | 0x80);

  outb(VGA_CRTC_INDEX, 0x11);
  val = inb(VGA_CRTC_DATA);
  outb(VGA_CRTC_DATA, val & ~0x80);

  regs[0x03] |= 0x80;
  regs[0x11] &= ~0x80;

  for(i = 0; i < 25; i++){
    outb(VGA_CRTC_INDEX, i);
    outb(VGA_CRTC_DATA, *regs);
    regs++;
  }

  // GC
  for(i = 0; i < 9; i++){
    outb(VGA_GC_INDEX, i);
    outb(VGA_GC_DATA, *regs);
    regs++;
  }

  // AC
  for(i = 0; i < 21; i++){
    inb(0x3DA);
    outb(VGA_AC_INDEX, i);
    outb(VGA_AC_WRITE, *regs);
    regs++;
  }

  inb(0x3DA);
  outb(VGA_AC_INDEX, 0x20);
}

// Mode 0x13 parameters
uchar mode_0x13[] = {
/* MISC */
	0x63,
/* SEQ */
	0x03, 0x01, 0x0F, 0x00, 0x0E,
/* CRTC */
	0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
	0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xE0, 0x9C, 0x8E, 0x28, 0x40, 0x96, 0xB9, 0xA3,
	0xFF,
/* GC */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
	0xFF,
/* AC */
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x41, 0x00, 0x0F, 0x00, 0x00
};

// Text mode 80x25 parameters (simplified, might need full dump)
// For now, we assume we can just switch to 0x13. Switching back might be hard without saving state.
// But xv6 starts in text mode, so we can maybe just reset?
// Actually, standard VGA BIOS call INT 10h is easiest but we are in protected mode.
// So we have to bang registers.

void
vga_set_mode(int mode)
{
  if(mode == 0x13){
    vga_write_regs(mode_0x13);
  }
}

void
vga_init(void)
{
}
