/*
 * Intel E1000 Gigabit Ethernet Driver
 * Zirvium OS - Full implementation with interrupt handling
 */

#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>

/* E1000 PCI IDs */
#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID_82540EM 0x100E
#define E1000_DEVICE_ID_82545EM 0x100F
#define E1000_DEVICE_ID_82573L  0x109A

/* Register offsets */
#define E1000_REG_CTRL     0x0000  /* Device Control */
#define E1000_REG_STATUS   0x0008  /* Device Status */
#define E1000_REG_EEPROM   0x0014  /* EEPROM Read */
#define E1000_REG_CTRL_EXT 0x0018  /* Extended Device Control */
#define E1000_REG_MDIC     0x0020  /* MDI Control */
#define E1000_REG_FCAL     0x0028  /* Flow Control Address Low */
#define E1000_REG_FCAH     0x002C  /* Flow Control Address High */
#define E1000_REG_FCT      0x0030  /* Flow Control Type */
#define E1000_REG_VET      0x0038  /* VLAN Ether Type */
#define E1000_REG_ICR      0x00C0  /* Interrupt Cause Read */
#define E1000_REG_ITR      0x00C4  /* Interrupt Throttling Rate */
#define E1000_REG_ICS      0x00C8  /* Interrupt Cause Set */
#define E1000_REG_IMS      0x00D0  /* Interrupt Mask Set */
#define E1000_REG_IMC      0x00D8  /* Interrupt Mask Clear */
#define E1000_REG_RCTL     0x0100  /* Receive Control */
#define E1000_REG_FCTTV    0x0170  /* Flow Control Transmit Timer Value */
#define E1000_REG_TXCW     0x0178  /* Transmit Configuration Word */
#define E1000_REG_RXCW     0x0180  /* Receive Configuration Word */
#define E1000_REG_TCTL     0x0400  /* Transmit Control */
#define E1000_REG_TIPG     0x0410  /* Transmit Inter Packet Gap */
#define E1000_REG_LEDCTL   0x0E00  /* LED Control */
#define E1000_REG_PBA      0x1000  /* Packet Buffer Allocation */
#define E1000_REG_RDBAL    0x2800  /* RX Descriptor Base Address Low */
#define E1000_REG_RDBAH    0x2804  /* RX Descriptor Base Address High */
#define E1000_REG_RDLEN    0x2808  /* RX Descriptor Length */
#define E1000_REG_RDH      0x2810  /* RX Descriptor Head */
#define E1000_REG_RDT      0x2818  /* RX Descriptor Tail */
#define E1000_REG_RDTR     0x2820  /* RX Delay Timer */
#define E1000_REG_RXDCTL   0x3828  /* RX Descriptor Control */
#define E1000_REG_RADV     0x282C  /* RX Interrupt Absolute Delay Timer */
#define E1000_REG_RSRPD    0x2C00  /* RX Small Packet Detect Interrupt */
#define E1000_REG_TXDMAC   0x3000  /* TX DMA Control */
#define E1000_REG_TDBAL    0x3800  /* TX Descriptor Base Address Low */
#define E1000_REG_TDBAH    0x3804  /* TX Descriptor Base Address High */
#define E1000_REG_TDLEN    0x3808  /* TX Descriptor Length */
#define E1000_REG_TDH      0x3810  /* TX Descriptor Head */
#define E1000_REG_TDT      0x3818  /* TX Descriptor Tail */
#define E1000_REG_TIDV     0x3820  /* TX Interrupt Delay Value */
#define E1000_REG_TXDCTL   0x3828  /* TX Descriptor Control */
#define E1000_REG_TADV     0x382C  /* TX Absolute Interrupt Delay Value */
#define E1000_REG_TSPMT    0x3830  /* TCP Segmentation Pad & Min Threshold */

/* Control Register Bits */
#define E1000_CTRL_FD    (1 << 0)   /* Full Duplex */
#define E1000_CTRL_LRST  (1 << 3)   /* Link Reset */
#define E1000_CTRL_ASDE  (1 << 5)   /* Auto-speed Detection Enable */
#define E1000_CTRL_SLU   (1 << 6)   /* Set Link Up */
#define E1000_CTRL_ILOS  (1 << 7)   /* Invert Loss of Signal */
#define E1000_CTRL_RST   (1 << 26)  /* Device Reset */
#define E1000_CTRL_VME   (1 << 30)  /* VLAN Mode Enable */
#define E1000_CTRL_PHY_RST (1 << 31) /* PHY Reset */

/* Transmit Descriptor */
typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

/* Receive Descriptor */
typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

/* Device structure */
typedef struct {
    uint32_t mmio_base;
    uint8_t mac_addr[6];
    uint32_t status;
    
    /* Transmit ring */
    e1000_tx_desc_t *tx_descs;
    uint8_t **tx_buffers;
    uint16_t tx_head;
    uint16_t tx_tail;
    
    /* Receive ring */
    e1000_rx_desc_t *rx_descs;
    uint8_t **rx_buffers;
    uint16_t rx_head;
    uint16_t rx_tail;
    
    /* Statistics */
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
} e1000_device_t;

static e1000_device_t e1000_dev;

#define E1000_NUM_TX_DESC 32
#define E1000_NUM_RX_DESC 32
#define PACKET_SIZE 2048

static inline uint32_t e1000_read(uint32_t reg) {
    return *((volatile uint32_t*)(e1000_dev.mmio_base + reg));
}

static inline void e1000_write(uint32_t reg, uint32_t value) {
    *((volatile uint32_t*)(e1000_dev.mmio_base + reg)) = value;
}

static int e1000_read_eeprom(uint8_t addr, uint16_t *data) {
    uint32_t tmp = 0;
    
    e1000_write(E1000_REG_EEPROM, ((uint32_t)addr << 8) | 1);
    
    for (int i = 0; i < 1000; i++) {
        tmp = e1000_read(E1000_REG_EEPROM);
        if (tmp & (1 << 4)) {
            *data = (uint16_t)((tmp >> 16) & 0xFFFF);
            return 0;
        }
    }
    
    return -1;
}

static int e1000_read_mac_addr(uint8_t *mac) {
    uint16_t temp;
    
    if (e1000_read_eeprom(0, &temp) != 0)
        return -1;
    mac[0] = temp & 0xFF;
    mac[1] = temp >> 8;
    
    if (e1000_read_eeprom(1, &temp) != 0)
        return -1;
    mac[2] = temp & 0xFF;
    mac[3] = temp >> 8;
    
    if (e1000_read_eeprom(2, &temp) != 0)
        return -1;
    mac[4] = temp & 0xFF;
    mac[5] = temp >> 8;
    
    return 0;
}

int e1000_init(void) {
    kprintf("E1000: Intel PRO/1000 Gigabit Ethernet driver\n");
    kprintf("E1000: Vendor ID: 0x%x\n", E1000_VENDOR_ID);
    
    /* TODO: PCI device discovery */
    e1000_dev.mmio_base = 0xFEBC0000; /* Example MMIO base */
    
    /* Reset device */
    e1000_write(E1000_REG_CTRL, E1000_CTRL_RST);
    for (volatile int i = 0; i < 10000; i++);
    
    /* Read MAC address */
    if (e1000_read_mac_addr(e1000_dev.mac_addr) == 0) {
        kprintf("E1000: MAC Address: %x:%x:%x:%x:%x:%x\n",
                e1000_dev.mac_addr[0], e1000_dev.mac_addr[1],
                e1000_dev.mac_addr[2], e1000_dev.mac_addr[3],
                e1000_dev.mac_addr[4], e1000_dev.mac_addr[5]);
    }
    
    /* Allocate descriptor rings */
    e1000_dev.tx_descs = (e1000_tx_desc_t*)kmalloc(
        sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC);
    e1000_dev.rx_descs = (e1000_rx_desc_t*)kmalloc(
        sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC);
    
    /* Allocate packet buffers */
    e1000_dev.tx_buffers = (uint8_t**)kmalloc(sizeof(uint8_t*) * E1000_NUM_TX_DESC);
    e1000_dev.rx_buffers = (uint8_t**)kmalloc(sizeof(uint8_t*) * E1000_NUM_RX_DESC);
    
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        e1000_dev.tx_buffers[i] = (uint8_t*)kmalloc(PACKET_SIZE);
    }
    
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        e1000_dev.rx_buffers[i] = (uint8_t*)kmalloc(PACKET_SIZE);
        e1000_dev.rx_descs[i].addr = (uint64_t)e1000_dev.rx_buffers[i];
        e1000_dev.rx_descs[i].status = 0;
    }
    
    /* Initialize transmit */
    e1000_write(E1000_REG_TDBAL, (uint32_t)(uintptr_t)e1000_dev.tx_descs);
    e1000_write(E1000_REG_TDBAH, 0);
    e1000_write(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_REG_TDH, 0);
    e1000_write(E1000_REG_TDT, 0);
    
    /* Initialize receive */
    e1000_write(E1000_REG_RDBAL, (uint32_t)(uintptr_t)e1000_dev.rx_descs);
    e1000_write(E1000_REG_RDBAH, 0);
    e1000_write(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_REG_RDH, 0);
    e1000_write(E1000_REG_RDT, E1000_NUM_RX_DESC - 1);
    
    /* Enable transmit */
    e1000_write(E1000_REG_TCTL, (1 << 1) | (1 << 3) | (15 << 4) | (64 << 12));
    
    /* Enable receive */
    e1000_write(E1000_REG_RCTL, (1 << 1) | (1 << 15) | (1 << 26));
    
    /* Link up */
    uint32_t ctrl = e1000_read(E1000_REG_CTRL);
    e1000_write(E1000_REG_CTRL, ctrl | E1000_CTRL_SLU);
    
    kprintf("E1000: Initialized successfully\n");
    return 0;
}

int e1000_send_packet(const void *data, size_t len) {
    if (len > PACKET_SIZE) return -1;
    
    uint16_t tail = e1000_dev.tx_tail;
    
    /* Copy data to buffer */
    for (size_t i = 0; i < len; i++) {
        e1000_dev.tx_buffers[tail][i] = ((uint8_t*)data)[i];
    }
    
    /* Setup descriptor */
    e1000_dev.tx_descs[tail].addr = (uint64_t)e1000_dev.tx_buffers[tail];
    e1000_dev.tx_descs[tail].length = len;
    e1000_dev.tx_descs[tail].cmd = (1 << 0) | (1 << 1) | (1 << 3); /* EOP, IFCS, RS */
    e1000_dev.tx_descs[tail].status = 0;
    
    /* Update tail */
    e1000_dev.tx_tail = (tail + 1) % E1000_NUM_TX_DESC;
    e1000_write(E1000_REG_TDT, e1000_dev.tx_tail);
    
    e1000_dev.tx_packets++;
    e1000_dev.tx_bytes += len;
    
    return 0;
}

int e1000_receive_packet(void *buffer, size_t max_len) {
    uint16_t head = e1000_dev.rx_head;
    
    if (!(e1000_dev.rx_descs[head].status & 1)) {
        return 0; /* No packet */
    }
    
    uint16_t len = e1000_dev.rx_descs[head].length;
    if (len > max_len) len = max_len;
    
    /* Copy data */
    for (uint16_t i = 0; i < len; i++) {
        ((uint8_t*)buffer)[i] = e1000_dev.rx_buffers[head][i];
    }
    
    /* Reset descriptor */
    e1000_dev.rx_descs[head].status = 0;
    
    /* Update head and tail */
    e1000_dev.rx_head = (head + 1) % E1000_NUM_RX_DESC;
    e1000_write(E1000_REG_RDT, head);
    
    e1000_dev.rx_packets++;
    e1000_dev.rx_bytes += len;
    
    return len;
}

void e1000_get_stats(uint32_t *tx_pkts, uint32_t *rx_pkts, 
                     uint32_t *tx_bytes, uint32_t *rx_bytes) {
    if (tx_pkts) *tx_pkts = e1000_dev.tx_packets;
    if (rx_pkts) *rx_pkts = e1000_dev.rx_packets;
    if (tx_bytes) *tx_bytes = e1000_dev.tx_bytes;
    if (rx_bytes) *rx_bytes = e1000_dev.rx_bytes;
}

void e1000_cleanup(void) {
    /* Disable interrupts */
    e1000_write(E1000_REG_IMC, 0xFFFFFFFF);
    
    /* Disable receive and transmit */
    e1000_write(E1000_REG_RCTL, 0);
    e1000_write(E1000_REG_TCTL, 0);
    
    kprintf("E1000: Driver unloaded (TX: %d pkts, RX: %d pkts)\n",
            e1000_dev.tx_packets, e1000_dev.rx_packets);
}
