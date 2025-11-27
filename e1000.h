#ifndef E1000_H
#define E1000_H

#define E1000_CTL      0x00000  /* Device Control Register - RW */
#define E1000_STATUS   0x00008  /* Device Status Register - RO */
#define E1000_EECD     0x00010  /* EEPROM/Flash Control - RW */
#define E1000_EERD     0x00014  /* EEPROM Read - RW */
#define E1000_ICR      0x000C0  /* Interrupt Cause Read - R/clr */
#define E1000_ICS      0x000C8  /* Interrupt Cause Set - WO */
#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */
#define E1000_IMC      0x000D8  /* Interrupt Mask Clear - WO */
#define E1000_IMS_RXT0 0x00000080  /* Receiver Timer Interrupt */
#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descriptor Tail - RW */
#define E1000_MTA      0x05200  /* Multicast Table Array - RW Array */
#define E1000_RA       0x05400  /* Receive Address - RW Array */

/* Device Control */
#define E1000_CTL_SLU    0x00000040    /* Set Link Up */
#define E1000_CTL_FRCSPD 0x00000800    /* Force Speed */
#define E1000_CTL_FRCDPLX 0x00001000   /* Force Duplex */
#define E1000_CTL_RST    0x04000000    /* Software Reset */

/* Transmit Control */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */

/* Receive Control */
#define E1000_RCTL_EN     0x00000002    /* enable rx */
#define E1000_RCTL_SBP    0x00000004    /* store bad packets */
#define E1000_RCTL_UPE    0x00000008    /* unicast promiscuous enable */
#define E1000_RCTL_MPE    0x00000010    /* multicast promiscuous enable */
#define E1000_RCTL_LPE    0x00000020    /* long packet enable */
#define E1000_RCTL_LBM_NO 0x00000000    /* no loopback mode */
#define E1000_RCTL_RDMTS_HALF 0x00000000 /* rx desc min threshold size */
#define E1000_RCTL_MO_36  0x00000000    /* multicast offset */
#define E1000_RCTL_BAM    0x00008000    /* broadcast accept mode */
#define E1000_RCTL_SZ_2048 0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SECRC  0x04000000    /* Strip Ethernet CRC */

/* Transmit Descriptor */
struct tx_desc {
  uint addr_low;
  uint addr_high;
  ushort length;
  uchar cso;
  uchar cmd;
  uchar status;
  uchar css;
  ushort special;
};

/* Transmit Descriptor Command Definitions */
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02 /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_RS     0x08 /* Report Status */

/* Transmit Descriptor Status Definitions */
#define E1000_TXD_STAT_DD    0x01 /* Descriptor Done */

/* Receive Descriptor */
struct rx_desc {
  uint addr_low;
  uint addr_high;
  ushort length;
  ushort checksum;
  uchar status;
  uchar errors;
  ushort special;
};

/* Receive Descriptor Status Definitions */
#define E1000_RXD_STAT_DD    0x01 /* Descriptor Done */
#define E1000_RXD_STAT_EOP   0x02 /* End of Packet */

#endif
