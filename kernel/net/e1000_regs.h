#ifndef ZIRVIUM_E1000_REGS_H
#define ZIRVIUM_E1000_REGS_H

#include <stdint.h>

#define E1000_NUM_RX_DESC  32
#define E1000_NUM_TX_DESC  32
#define E1000_RX_BUF_SIZE  2048

/* PCI vendor/device IDs for Intel 82540EM */
#define E1000_VENDOR  0x8086
#define E1000_DEVICE  0x100E

/* Register offsets */
#define E1000_CTRL      0x0000
#define E1000_STATUS    0x0008
#define E1000_EECD      0x0010
#define E1000_EERD      0x0014
#define E1000_CTRL_EXT  0x0018
#define E1000_ICR       0x00C0
#define E1000_IMS       0x00D0
#define E1000_RCTL      0x0100
#define E1000_FCTRL     0x0508
#define E1000_TCTL      0x0400
#define E1000_TIPG      0x0410
#define E1000_RDBAL     0x2800
#define E1000_RDBAH     0x2804
#define E1000_RDLEN     0x2808
#define E1000_RDH       0x2810
#define E1000_RDT       0x2818
#define E1000_TDBAL     0x3800
#define E1000_TDBAH     0x3804
#define E1000_TDLEN     0x3808
#define E1000_TDH       0x3810
#define E1000_TDT       0x3818
#define E1000_MTA       0x5200
#define E1000_RA        0x5400    /* first MAC address entry (16 bytes) */
#define E1000_RA2       0x5408    /* second half of first MAC entry */

/* CTRL bits */
#define E1000_CTRL_FD       (1 << 0)
#define E1000_CTRL_LRST     (1 << 3)
#define E1000_CTRL_ASDE     (1 << 5)
#define E1000_CTRL_SLU      (1 << 6)
#define E1000_CTRL_ILOS     (1 << 7)
#define E1000_CTRL_RST      (1 << 26)
#define E1000_CTRL_VME      (1 << 30)
#define E1000_CTRL_PHY_RST  (1 << 31)

/* STATUS bits */
#define E1000_STATUS_LU      (1 << 1)

/* RCTL bits */
#define E1000_RCTL_EN        (1 << 1)
#define E1000_RCTL_SBP       (1 << 2)
#define E1000_RCTL_UPE       (1 << 3)
#define E1000_RCTL_MPE       (1 << 4)
#define E1000_RCTL_LPE       (1 << 5)
#define E1000_RCTL_LBM_NONE  (0 << 6)
#define E1000_RCTL_LBM_PHY   (3 << 6)
#define E1000_RCTL_RDMTS_1   (0 << 8)
#define E1000_RCTL_RDMTS_HALF (1 << 8)
#define E1000_RCTL_RDMTS_QUAR (2 << 8)
#define E1000_RCTL_MO_SHIFT  12
#define E1000_RCTL_BAM       (1 << 15)
/* RCTL buffer size encodings — unused, kept for reference */
#define E1000_RCTL_SECRC     (1 << 26)

/* TCTL bits */
#define E1000_TCTL_EN        (1 << 1)
#define E1000_TCTL_PSP       (1 << 3)
#define E1000_TCTL_CT_SHIFT  4
#define E1000_TCTL_COLD_SHIFT 12
#define E1000_TCTL_SWXOFF    (1 << 22)

/* TX descriptor command bits */
#define E1000_TXD_CMD_EOP    (1 << 0)
#define E1000_TXD_CMD_IFCS   (1 << 1)
#define E1000_TXD_CMD_IC     (1 << 2)
#define E1000_TXD_CMD_RS     (1 << 3)
#define E1000_TXD_CMD_RPS    (1 << 4)
#define E1000_TXD_CMD_DEXT   (1 << 5)
#define E1000_TXD_CMD_VLE    (1 << 6)
#define E1000_TXD_CMD_IDE    (1 << 7)

/* TX descriptor status bits */
#define E1000_TXD_STAT_DD    (1 << 0)
#define E1000_TXD_STAT_EC    (1 << 1)
#define E1000_TXD_STAT_LC    (1 << 2)

/* RX descriptor status bits */
#define E1000_RXD_STAT_DD    (1 << 0)
#define E1000_RXD_STAT_EOP   (1 << 1)

/* EEPROM Read register (EERD) */
#define E1000_EERD_START    (1 << 0)
#define E1000_EERD_DONE     (1 << 4)
#define E1000_EERD_ADDR_SHIFT 8
#define E1000_EERD_DATA_SHIFT 16

/* Legacy descriptor structures */
typedef struct e1000_rx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t csum;
    volatile uint8_t  status;
    volatile uint8_t  errors;
    volatile uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct e1000_tx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t  cso;
    volatile uint8_t  cmd;
    volatile uint8_t  status;
    volatile uint8_t  css;
    volatile uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

#endif
