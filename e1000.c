#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "net.h"
#include "e1000.h"

#define TX_RING_SIZE 16
#define RX_RING_SIZE 16

struct spinlock e1000_lock;

struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));

struct mbuf *rx_mbufs[RX_RING_SIZE];
struct mbuf *tx_mbufs[TX_RING_SIZE];

volatile uint *e1000_regs;

// PCI definitions
#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

#define PCI_VENDOR_ID 0x8086
#define PCI_DEVICE_ID_E1000 0x100E

static uint pci_read(int bus, int dev, int func, int reg) {
  uint addr = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (reg & 0xFC);
  outl(PCI_ADDR, addr);
  return inl(PCI_DATA);
}

static void e1000_scan_pci() {
  int bus, dev, func;
  for (bus = 0; bus < 256; bus++) {
    for (dev = 0; dev < 32; dev++) {
      for (func = 0; func < 8; func++) {
        uint id = pci_read(bus, dev, func, 0);
        if ((id & 0xFFFF) == PCI_VENDOR_ID && ((id >> 16) & 0xFFFF) == PCI_DEVICE_ID_E1000) {
          uint bar0 = pci_read(bus, dev, func, 0x10);
          uint phys_addr = bar0 & ~0xF;
          
          // E1000 MMIO is identity-mapped in DEVSPACE region
          e1000_regs = (uint *)phys_addr;
          
          // Enable bus mastering
          uint cmd = pci_read(bus, dev, func, 0x04);
          outl(PCI_ADDR, (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | 0x04);
          outl(PCI_DATA, cmd | 0x4);
          
          cprintf("E1000: Found at bus %d dev %d func %d, BAR0 %p\n", bus, dev, func, e1000_regs);
          return;
        }
      }
    }
  }
  cprintf("e1000: not found\n");
}

void e1000_init(void) {
  initlock(&e1000_lock, "e1000");
  e1000_scan_pci();
  if(!e1000_regs)
    return;

  // Reset
  e1000_regs[E1000_IMS >> 2] = 0; // Disable interrupts
  e1000_regs[E1000_CTL >> 2] |= E1000_CTL_RST;
  e1000_regs[E1000_IMS >> 2] = 0; // Disable interrupts again after reset

  // Link setup
  e1000_regs[E1000_CTL >> 2] |= E1000_CTL_SLU | E1000_CTL_FRCSPD | E1000_CTL_FRCDPLX;

  // RX Setup
  for (int i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000_init: mbufalloc failed");
    rx_ring[i].addr_low = V2P(rx_mbufs[i]->head);
    rx_ring[i].addr_high = 0;
    rx_ring[i].status = 0;
  }

  e1000_regs[E1000_RDBAL >> 2] = V2P(rx_ring);
  e1000_regs[E1000_RDBAH >> 2] = 0;
  e1000_regs[E1000_RDLEN >> 2] = sizeof(rx_ring);
  e1000_regs[E1000_RDH >> 2] = 0;
  e1000_regs[E1000_RDT >> 2] = RX_RING_SIZE - 1;
  e1000_regs[E1000_RCTL >> 2] = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048 | E1000_RCTL_SECRC;

  // TX Setup
  for (int i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }

  e1000_regs[E1000_TDBAL >> 2] = V2P(tx_ring);
  e1000_regs[E1000_TDBAH >> 2] = 0;
  e1000_regs[E1000_TDLEN >> 2] = sizeof(tx_ring);
  e1000_regs[E1000_TDH >> 2] = 0;
  e1000_regs[E1000_TDT >> 2] = 0;
  e1000_regs[E1000_TCTL >> 2] = E1000_TCTL_EN | E1000_TCTL_PSP;

  // Enable interrupts
  e1000_regs[E1000_IMS >> 2] = E1000_IMS_RXT0;
}

void e1000_transmit(struct mbuf *m) {
  if(!e1000_regs){
    mbuffree(m);
    return;
  }
  acquire(&e1000_lock);
  uint tail = e1000_regs[E1000_TDT >> 2];
  struct tx_desc *desc = &tx_ring[tail];
  
  if ((desc->status & E1000_TXD_STAT_DD) == 0) {
    // Ring full
    release(&e1000_lock);
    mbuffree(m);
    return;
  }
  
  if (tx_mbufs[tail])
    mbuffree(tx_mbufs[tail]);
  
  tx_mbufs[tail] = m;
  desc->addr_low = V2P(m->head);
  desc->addr_high = 0;
  desc->length = m->len;
  desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  desc->status = 0;
  
  e1000_regs[E1000_TDT >> 2] = (tail + 1) % TX_RING_SIZE;
  release(&e1000_lock);
}

static void e1000_recv(void) {
  uint tail = (e1000_regs[E1000_RDT >> 2] + 1) % RX_RING_SIZE;
  struct rx_desc *desc = &rx_ring[tail];
  
  while ((desc->status & E1000_RXD_STAT_DD)) {
    struct mbuf *m = rx_mbufs[tail];
    mbufput(m, desc->length);
    
    // Allocate new mbuf for this slot
    rx_mbufs[tail] = mbufalloc(0);
    if (!rx_mbufs[tail])
      panic("e1000_recv: mbufalloc failed");
    
    desc->addr_low = V2P(rx_mbufs[tail]->head);
    desc->addr_high = 0;
    desc->status = 0;
    
    e1000_regs[E1000_RDT >> 2] = tail;
    
    tail = (tail + 1) % RX_RING_SIZE;
    desc = &rx_ring[tail];
    
    net_rx(m);
  }
}

void e1000_intr(void) {
  if(!e1000_regs)
    return;
  e1000_regs[E1000_ICR >> 2]; // Read to clear
  e1000_recv();
}
