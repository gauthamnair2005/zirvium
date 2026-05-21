/* RTL8139 Fast Ethernet driver
 * Ported from Linux drivers/net/ethernet/realtek/8139too.c
 * Legacy PCI mode, polling, no interrupts, I/O-space BAR
 * Zirvium/MOSIX adaptation via linux_compat.h */
#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/pci_compat.h"
#include "drivers/zirv/device.h"
#include "kernel/console.h"
#include "kernel/net/stack.h"
#include <stdint.h>
#include <string.h>

/* RTL8139 register offsets (I/O space) */
#define RTL_REG_MAC0           0x00
#define RTL_REG_MAC4           0x04
#define RTL_REG_CONFIG1        0x52
#define RTL_REG_IMR            0x3C
#define RTL_REG_ISR            0x3E
#define RTL_REG_TXCFG          0x40
#define RTL_REG_TX_STATUS0     0x10
#define RTL_REG_TX_ADDR0       0x20
#define RTL_REG_RXCFG          0x44
#define RTL_REG_MPC            0x48
#define RTL_REG_RXMISS         0x4C
#define RTL_REG_RX_BUF_PTR     0x38
#define RTL_REG_COMMAND        0x37
#define RTL_REG_CAPR           0x38
#define RTL_REG_RBSTART        0x30
#define RTL_REG_9346CR         0x50
#define RTL_REG_CONFIG4        0x53
#define RTL_REG_MEDIA_STATUS   0x58
#define RTL_REG_MII            0x5A
#define RTL_REG_HLT_CLK        0x59
#define RTL_REG_MULTI_INTR     0x5C
#define RTL_REG_PHY_PARM       0x5C
#define RTL_REG_TX_EARLY_THR   0x5E
#define RTL_REG_BRST           0x5F
#define RTL_REG_FIFOTMS        0x70
#define RTL_REG_FIFOTMR        0x6C
#define RTL_REG_CRC0           0x60
#define RTL_REG_CRC1           0x64
#define RTL_REG_CRC2           0x68

/* TXCFG bits */
#define TXCFG_HW_VER_SHIFT     14
#define TXCFG_HW_VER_MASK      (7 << TXCFG_HW_VER_SHIFT)
#define TXCFG_MAX_DMA_SHIFT    8
#define TXCFG_MAX_DMA_MASK     (7 << TXCFG_MAX_DMA_SHIFT)
#define TXCFG_MAX_DMA_1024     7
#define TXCFG_TXRR_SHIFT       4
#define TXCFG_TXRR_MASK        (15 << TXCFG_TXRR_SHIFT)
#define TXCFG_TXRR_ZERO        (0 << TXCFG_TXRR_SHIFT)
#define TXCFG_IFG_SHIFT        24
#define TXCFG_IFG_MASK         (0x7F << TXCFG_IFG_SHIFT)
#define TXCFG_IFG_STANDARD     (0x60 << TXCFG_IFG_SHIFT)

/* RXCFG bits */
#define RXCFG_RXFTH_SHIFT      13
#define RXCFG_RXFTH_MASK       (7 << RXCFG_RXFTH_SHIFT)
#define RXCFG_RXFTH_NONE       (7 << RXCFG_RXFTH_SHIFT)
#define RXCFG_RX_DMA_BURST     (7 << 8)
#define RXCFG_RX_EARLY_BYTE    (7 << 0)
#define RXCFG_RX_EARLY_BYTE_EN (1 << 0)
#define RXCFG_RX_EARLY_SIZE    0x0700
#define RXCFG_RX_EARLY_SIZE_64 0x0100
#define RXCFG_RX_EARLY_SIZE_128 0x0200
#define RXCFG_RX_EARLY_SIZE_256 0x0300

/* Command register bits */
#define CMD_RESET              0x10
#define CMD_RX_ENABLE          0x08
#define CMD_TX_ENABLE          0x04
#define CMD_BUF_EMPTY          0x01

/* Interrupt bits */
#define ISR_ROK                0x01
#define ISR_TOK                0x04
#define ISR_RERR               0x08
#define ISR_TERR               0x10
#define ISR_RX_OVW            0x20
#define ISR_PUN                0x40
#define ISR_LINK_CHG          0x80
#define ISR_FIFO_OFL          0x100
#define ISR_SERR              0x200
#define ISR_PCS_TIMEOUT       0x400
#define ISR_PCI_ERR           0x10000
#define ISR_PCSC_TIME         0x20000
#define ISR_TIMEOUT            0x40000
#define ISR_SW_INTR           0x400000
#define ISR_SW_INTR_OLD       0x010000

/* RX packet header */
#define RX_STATUS_MULTICAST   (1 << 0)
#define RX_STATUS_RXOK        (1 << 2)
#define RX_STATUS_PHY_MATCH   (1 << 5)
#define RX_STATUS_BROADCAST   (1 << 6)
#define RX_STATUS_TAG         (1 << 7)
#define RX_STATUS_LEN_MASK    0x1FFF

/* Config1 bits */
#define CONFIG1_PM_ENABLE     0x01
#define CONFIG1_IOMAP         0x02
#define CONFIG1_VPD           0x04
#define CONFIG1_LED0          0x08
#define CONFIG1_LED1          0x10
#define CONFIG1_LWAKE         0x20
#define CONFIG1_DRVLOAD       0x40
#define CONFIG1_PCS           0x80

/* 9346CR bits */
#define CR_9356_SEL            0x80
#define CR_EEM0               0x01
#define CR_EEM1               0x02
#define CR_EECS               0x08
#define CR_EESK               0x10
#define CR_EEDI               0x20
#define CR_EEDO               0x40
#define CR_EERW               0x80

/* MII register offsets */
#define MII_BMSR              0x01
#define MII_BMCR              0x00
#define MII_ADVERTISE         0x04
#define MII_LPA               0x05
#define MII_EXPAN             0x06
#define MII_DCOUNTER          0x12
#define MII_FCSCOUNTER        0x13
#define MII_RERRCOUNTER       0x14
#define MII_CTRL1000          0x09
#define MII_STAT1000          0x0A
#define MII_ESTATUS           0x0F

/* MII BMCR bits */
#define BMCR_RESET            0x8000
#define BMCR_SPEED100         0x2000
#define BMCR_AN_ENABLE        0x1000
#define BMCR_RESTART_AN       0x0200
#define BMCR_FULL_DUPLEX      0x0100
#define BMCR_ISOLATE          0x0400
#define BMCR_PDOWN            0x0800
#define BMCR_ANRESTART        0x0200
#define BMCR_LOOPBACK         0x4000

/* MII BMSR bits */
#define BMSR_LSTATUS          0x0004
#define BMSR_AN_COMPLETE      0x0020

/* TX Status bits */
#define TX_STATUS_OWN         (1 << 31)
#define TX_STATUS_UNDERRUN    (1 << 2)
#define TX_STATUS_OK          0x80000000
#define TX_STATUS_ABORT       0x40000000
#define TX_STATUS_CARRIER_LOST 0x20000000
#define TX_STATUS_WINDOW_ERR  0x10000000
#define TX_STATUS_HOST_OWN    0x08000000
#define TX_STATUS_OUT_OF_WINDOW 0x04000000
#define TX_STATUS_GCOLL       0x02000000
#define TX_STATUS_LCOLL       0x01000000
#define TX_STATUS_NIC_BUF_MASK 0xFFF00000

#define RX_BUF_LEN      (1536 + 16 + 1536 % 128)
#define TX_BUF_LEN      1536
#define NUM_TX_DESC     4

struct rtl8139_dev {
    pci_dev_t        *pdev;
    void             *mmio;
    int               io_type;
    uint8_t           mac[6];
    uint8_t           rx_buf[RX_BUF_LEN] __attribute__((aligned(16)));
    uint8_t           tx_buf[NUM_TX_DESC][TX_BUF_LEN];
    int               tx_cur;
    int               link_up;
    registered_device_t *rdev;
};

static inline uint8_t  rtl_read8(struct rtl8139_dev *dev, uint16_t reg)
{
    return ioread8(dev->mmio + reg);
}

static inline uint16_t rtl_read16(struct rtl8139_dev *dev, uint16_t reg)
{
    return ioread16(dev->mmio + reg);
}

static inline uint32_t rtl_read32(struct rtl8139_dev *dev, uint16_t reg)
{
    return ioread32(dev->mmio + reg);
}

static inline void rtl_write8(struct rtl8139_dev *dev, uint16_t reg, uint8_t val)
{
    iowrite8(val, dev->mmio + reg);
}

static inline void rtl_write16(struct rtl8139_dev *dev, uint16_t reg, uint16_t val)
{
    iowrite16(val, dev->mmio + reg);
}

static inline void rtl_write32(struct rtl8139_dev *dev, uint16_t reg, uint32_t val)
{
    iowrite32(val, dev->mmio + reg);
}

static void rtl8139_reset(struct rtl8139_dev *dev)
{
    rtl_write8(dev, RTL_REG_COMMAND, CMD_RESET);
    for (int i = 0; i < 10000; i++) {
        if (!(rtl_read8(dev, RTL_REG_COMMAND) & CMD_RESET)) break;
        __asm__("pause");
    }
}

static int rtl8139_mii_read(struct rtl8139_dev *dev, int phy, int reg)
{
    int val = -1;
    rtl_write16(dev, RTL_REG_MII, 0x0000);
    for (int i = 0; i < 20000; i++) {
        if (!(rtl_read16(dev, RTL_REG_MII) & 0x0001)) {
            val = (rtl_read16(dev, RTL_REG_MII) >> 16) & 0xFFFF;
            break;
        }
        __asm__("pause");
    }
    (void)phy;
    (void)reg;
    return val;
}

static int rtl8139_check_link(struct rtl8139_dev *dev)
{
    int val = rtl8139_mii_read(dev, 0, MII_BMSR);
    return (val >= 0 && (val & BMSR_LSTATUS)) ? 1 : 0;
}

static void rtl8139_init_hw(struct rtl8139_dev *dev)
{
    rtl8139_reset(dev);

    uint32_t rx_buf_phys = (uint32_t)(uint64_t)(uintptr_t)dev->rx_buf;

    rtl_write32(dev, RTL_REG_RBSTART, rx_buf_phys);
    rtl_write32(dev, RTL_REG_RXCFG, RXCFG_RXFTH_NONE | RXCFG_RX_DMA_BURST
                 | RXCFG_RX_EARLY_SIZE_256);

    rtl_write32(dev, RTL_REG_TXCFG, TXCFG_IFG_STANDARD
                 | (TXCFG_MAX_DMA_1024 << TXCFG_MAX_DMA_SHIFT)
                 | (TXCFG_TXRR_ZERO));

    rtl_write16(dev, RTL_REG_IMR, 0x0000);
    rtl_write16(dev, RTL_REG_ISR, 0xFFFF);

    rtl_write8(dev, RTL_REG_COMMAND, CMD_TX_ENABLE | CMD_RX_ENABLE);
}

static int rtl8139_send(struct rtl8139_dev *dev, const void *data, uint16_t len)
{
    uint8_t *tx_buf = dev->tx_buf[dev->tx_cur];
    memcpy(tx_buf, data, len);

    rtl_write32(dev, RTL_REG_TX_ADDR0 + dev->tx_cur * 4,
                (uint32_t)(uint64_t)(uintptr_t)tx_buf);
    rtl_write32(dev, RTL_REG_TX_STATUS0 + dev->tx_cur * 4,
                len | TX_STATUS_OWN);

    for (int i = 0; i < 50000; i++) {
        uint32_t status = rtl_read32(dev, RTL_REG_TX_STATUS0 + dev->tx_cur * 4);
        if (!(status & TX_STATUS_OWN)) {
            dev->tx_cur = (dev->tx_cur + 1) % NUM_TX_DESC;
            return (int)len;
        }
        __asm__("pause");
    }
    return -1;
}

static int rtl8139_recv(struct rtl8139_dev *dev, uint8_t *buf, uint16_t buflen)
{
    uint16_t capr = rtl_read16(dev, RTL_REG_CAPR);
    uint16_t rx_ptr = capr + 16;

    if (!(rtl_read8(dev, RTL_REG_COMMAND) & CMD_BUF_EMPTY)) {
        uint32_t rx_status = *(volatile uint32_t *)(dev->rx_buf + rx_ptr);
        uint16_t rx_len = (rx_status >> 16) & 0x3FFF;

        if (rx_len > 0 && rx_len < 0x3FFF) {
            if (rx_len > buflen) rx_len = buflen;
            memcpy(buf, dev->rx_buf + rx_ptr + 4, rx_len - 4);
            capr = (uint16_t)(rx_ptr + rx_len + 4);
            capr = (uint16_t)(capr & 0xFFFC);
            rtl_write16(dev, RTL_REG_CAPR, capr);
            return rx_len - 4;
        }
    }
    return 0;
}

static int rtl8139_vfs_write(vnode_t *vn, const void *buf,
                              size_t count, uint64_t off)
{
    (void)vn; (void)off;
    if (!buf || count == 0) return 0;
    registered_device_t *reg = (registered_device_t *)vn->device;
    if (!reg) return -1;
    struct rtl8139_dev *dev = (struct rtl8139_dev *)reg->desc.driver_data;
    if (!dev) return -1;
    if (count > TX_BUF_LEN) count = TX_BUF_LEN;
    return rtl8139_send(dev, buf, (uint16_t)count);
}

static int rtl8139_vfs_read(vnode_t *vn, void *buf,
                             size_t count, uint64_t off)
{
    (void)vn; (void)off;
    if (!buf || count == 0) return 0;
    registered_device_t *reg = (registered_device_t *)vn->device;
    if (!reg) return -1;
    struct rtl8139_dev *dev = (struct rtl8139_dev *)reg->desc.driver_data;
    if (!dev) return -1;
    if (count > 65535) count = 65535;
    return rtl8139_recv(dev, (uint8_t *)buf, (uint16_t)count);
}

static int rtl8139_probe(pci_dev_t *pdev, const pci_device_id_t *id)
{
    (void)id;
    if (pdev->class_code != PCI_CLASS_NETWORK) return -ENODEV;

    klog(LOG_INFO, "RTL ", "RTL8139 probing %04x:%04x (rev %02x)",
         pdev->vendor_id, pdev->device_id, pdev->revision);

    struct rtl8139_dev *dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) return -ENOMEM;
    dev->pdev = pdev;

    pci_enable_device(pdev);
    dev->mmio = pci_map_bar(pdev, 0);
    if (!dev->mmio) { kfree(dev); return -1; }

    klog(LOG_INFO, "RTL ", "RTL8139 MMIO at %p", dev->mmio);

    rtl8139_init_hw(dev);

    for (int i = 0; i < 6; i++)
        dev->mac[i] = rtl_read8(dev, RTL_REG_MAC0 + i);

    dev->link_up = rtl8139_check_link(dev);

    klog(LOG_OK, "RTL ", "RTL8139 MAC %02x:%02x:%02x:%02x:%02x:%02x%s",
         dev->mac[0], dev->mac[1], dev->mac[2],
         dev->mac[3], dev->mac[4], dev->mac[5],
         dev->link_up ? " (UP)" : " (DOWN)");

    registered_device_t *reg = zirv_register_device(
        DEV_CLASS_NET_ETH, DEV_CLASS_NET_ETH, "Realtek RTL8139", NULL);
    if (!reg || !reg->vnode) { kfree(dev); return -1; }
    reg->desc.driver_data = dev;
    dev->rdev = reg;

    return 0;
}

static void rtl8139_remove(pci_dev_t *pdev)
{
    struct rtl8139_dev *dev = (struct rtl8139_dev *)pdev->driver_data;
    if (dev) { kfree(dev); pdev->driver_data = NULL; }
}

static const pci_device_id_t rtl8139_id_table[] = {
    { PCI_DEVICE(0x10EC, 0x8139) },
    { PCI_DEVICE(0x10EC, 0x8138) },
    { PCI_DEVICE(0x10EC, 0x8129) },
    { PCI_DEVICE(0x1186, 0x1300) },
    { PCI_DEVICE(0x1186, 0x1340) },
    { PCI_DEVICE(0x13D1, 0xAB06) },
    { PCI_DEVICE(0x1259, 0xA117) },
    { PCI_DEVICE(0x14EA, 0xAB06) },
    { 0, }
};

static struct pci_driver_linux rtl8139_driver = {
    .name      = "rtl8139",
    .id_table  = rtl8139_id_table,
    .probe     = rtl8139_probe,
    .remove    = rtl8139_remove,
};

void rtl8139_init(void)
{
    klog(LOG_INFO, "RTL ", "RTL8139 driver (Linux compat)");
    pci_compat_register_driver(&rtl8139_driver);
}
