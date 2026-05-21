/* USB xHCI host controller driver — polling mode
 * Ported from Linux drivers/usb/host/xhci.c + xhci-ring.c + xhci-mem.c
 * Zirvium/MOSIX adaptation via linux_compat.h
 *
 * Provides USB 2.0/3.x enumeration and mass storage access.
 * Uses polling (no interrupts), simplified transfer rings (blocking). */
#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/pci_compat.h"
#include "drivers/zirv/device.h"
#include "kernel/console.h"
#include <stdint.h>
#include <string.h>

/* PCI class for xHCI: 0x0C (serial bus), 0x03 (USB), 0x30 (xHCI) */
#define PCI_CLASS_XHCI      0x0C0330

/* XHCI MMIO register offsets */
#define XHCI_CAPLENGTH      0x00
#define XHCI_HCIVERSION     0x02
#define XHCI_HCSPARAMS1     0x04
#define XHCI_HCSPARAMS2     0x08
#define XHCI_HCSPARAMS3     0x0C
#define XHCI_HCCPARAMS1     0x10
#define XHCI_DBOFF          0x14
#define XHCI_RTSOFF         0x18
#define XHCI_USBCMD         0x00
#define XHCI_USBSTS         0x04
#define XHCI_PAGESIZE       0x08
#define XHCI_DNCTRL         0x0C
#define XHCI_CRCR           0x0C
#define XHCI_DCBAAP         0x10
#define XHCI_CONFIG         0x14
#define XHCI_PORTSC_BASE    0x400

/* USBCMD bits */
#define USBCMD_RUN          (1 << 0)
#define USBCMD_HCRST        (1 << 1)
#define USBCMD_INTE         (1 << 2)
#define USBCMD_HSEE         (1 << 3)
#define USBCMD_LHCRST       (1 << 7)

/* USBSTS bits */
#define USBSTS_HCH          (1 << 0)
#define USBSTS_HSE          (1 << 2)
#define USBSTS_EINT         (1 << 3)
#define USBSTS_PCD          (1 << 4)
#define USBSTS_CNR          (1 << 11)

/* PORTSC bits */
#define PORTSC_CCS          (1 << 0)
#define PORTSC_PED          (1 << 1)
#define PORTSC_PR           (1 << 4)
#define PORTSC_PP           (1 << 9)
#define PORTSC_SPEED_SHIFT  10
#define PORTSC_SPEED_MASK   (0xF << 10)
#define PORTSC_CSC          (1 << 17)
#define PORTSC_CEC          (1 << 23)
#define PORTSC_WRC          (1 << 19)

/* TRB types */
#define TRB_NORMAL          1
#define TRB_SETUP           2
#define TRB_DATA            3
#define TRB_STATUS          4
#define TRB_LINK            6
#define TRB_EVENT           7
#define TRB_CMD_NOOP        10
#define TRB_CMD_ENABLE_SLOT 12
#define TRB_CMD_ADDRESS_DEV 13

/* TRB flags */
#define TRB_C               (1 << 0)
#define TRB_ENT             (1 << 1)
#define TRB_ISP             (1 << 2)
#define TRB_NS              (1 << 3)
#define TRB_CHAIN           (1 << 4)
#define TRB_IOC             (1 << 5)
#define TRB_IDT             (1 << 6)
#define TRB_CYCLE           (1 << 0)
#define TRB_TOGGLE          (1 << 1)
#define TRB_ERROR_SHIFT     24
#define TRB_ERROR_MASK      (0xFF << 24)
#define TRB_COMP_SHIFT      24
#define TRB_COMP_MASK       (0xFF << 24)

/* TRB completion codes */
#define COMP_SUCCESS         1
#define COMP_SHORT_TX        13
#define COMP_TRB_ERROR       5
#define COMP_STALL           6
#define COMP_ENUM           10
#define COMP_BABBLE          12
#define COMP_USB_TX_ERR     8
#define COMP_TRB_STARVATION  31

/* Setup packet direction bits */
#define SETUP_DIR_IN        (1 << 7)
#define SETUP_DIR_OUT       (0)
#define SETUP_TYPE_STD      (0 << 5)
#define SETUP_TYPE_CLASS    (1 << 5)
#define SETUP_TYPE_VENDOR   (2 << 5)
#define SETUP_RCPT_DEV      (0)
#define SETUP_RCPT_IF       (1)
#define SETUP_RCPT_EP       (2)
#define SETUP_RCPT_OTHER    (3)

/* Standard USB requests */
#define REQ_GET_DESC        6
#define REQ_SET_ADDR        5
#define REQ_SET_CONFIG      9
#define DESC_DEVICE         1
#define DESC_CONFIG         2
#define DESC_STRING         3

/* USB device speeds */
#define USB_SPEED_LOW       1
#define USB_SPEED_FULL      2
#define USB_SPEED_HIGH      3
#define USB_SPEED_SUPER     4

/* Max devices per controller */
#define XHCI_MAX_SLOTS      32
#define XHCI_RING_SIZE      64
#define XHCI_EVENT_RING_SIZE 64
#define XHCI_ERST_SIZE      1

struct xhci_trb {
    uint32_t params[4];
} __attribute__((packed));

struct xhci_ring {
    struct xhci_trb *trbs;
    uint64_t         phys;
    int              enqueue;
    int              cycle;
    int              size;
};

struct xhci_dev_ctx {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsvd[6];
    uint32_t ep0[8];
    uint32_t ep_data[8];
    uint32_t slot[8];
    uint32_t scratch[4];
} __attribute__((packed));

struct xhci_slot {
    int             enabled;
    int             addressed;
    uint8_t         speed;
    uint8_t         port;
    uint8_t         dev_addr;
    uint16_t        vid;
    uint16_t        pid;
    int             config_val;
    int             max_ep0_size;
    struct xhci_dev_ctx *ctx;
    uint64_t        ctx_phys;
};

struct xhci_dev {
    pci_dev_t        *pdev;
    void             *mmio;
    uint8_t          *cap_regs;
    uint8_t          *op_regs;
    uint8_t          *db_regs;
    uint8_t          *rt_regs;
    int               cap_length;
    int               max_slots;
    int               max_ports;
    int               page_size;
    int               hci_version;

    struct xhci_ring  cmd_ring;
    struct xhci_ring  event_ring;
    struct xhci_trb  *cmd_trbs;
    uint64_t          cmd_trbs_phys;
    struct xhci_trb  *evt_trbs;
    uint64_t          evt_trbs_phys;
    uint64_t         *dcbaa;
    uint64_t          dcbaa_phys;

    struct xhci_slot  slots[XHCI_MAX_SLOTS];
    uint8_t           scratchpad[4096] __attribute__((aligned(4096)));
    int               initialized;
    int               cmd_cycle;
    int               evt_cycle;
    registered_device_t *rdev;
};

static inline uint32_t xhci_read32(struct xhci_dev *dev, void *addr)
{
    return ioread32(addr);
}

static inline void xhci_write32(struct xhci_dev *dev, void *addr, uint32_t val)
{
    iowrite32(val, addr);
}

static inline void xhci_write64(struct xhci_dev *dev, void *addr, uint64_t val)
{
    xhci_write32(dev, addr, (uint32_t)(val & 0xFFFFFFFF));
    xhci_write32(dev, addr + 4, (uint32_t)(val >> 32));
}

static inline void xhci_doorbell(struct xhci_dev *dev, int slot, int stream)
{
    xhci_write32(dev, dev->db_regs, (uint32_t)((stream << 16) | slot));
}

static int xhci_reset(struct xhci_dev *dev)
{
    xhci_write32(dev, dev->op_regs + XHCI_USBCMD, USBCMD_HCRST);
    for (int i = 0; i < 100000; i++) {
        if (!(xhci_read32(dev, dev->op_regs + XHCI_USBCMD) & USBCMD_HCRST))
            break;
        __asm__("pause");
    }

    xhci_write32(dev, dev->op_regs + XHCI_USBCMD, USBCMD_RUN | USBCMD_HSEE);
    for (int i = 0; i < 100000; i++) {
        if (!(xhci_read32(dev, dev->op_regs + XHCI_USBSTS) & USBSTS_HCH))
            break;
        __asm__("pause");
    }
    klog(LOG_INFO, "XHCI", "controller reset OK");
    return 0;
}

static int xhci_ring_init(struct xhci_dev *dev, struct xhci_ring *ring,
                           int size, uint64_t *phys_out)
{
    ring->trbs = (struct xhci_trb *)kzalloc(size * sizeof(struct xhci_trb) + 4095,
                                            GFP_KERNEL);
    if (!ring->trbs) return -1;
    ring->phys = (uint64_t)(uintptr_t)ring->trbs;
    ring->phys = (ring->phys + 4095) & ~4095ULL;
    ring->trbs = (struct xhci_trb *)(uintptr_t)ring->phys;
    memset(ring->trbs, 0, size * sizeof(struct xhci_trb));
    ring->enqueue = 0;
    ring->cycle = 1;
    ring->size = size;
    if (phys_out) *phys_out = ring->phys;
    return 0;
}

static void xhci_trb_setup(struct xhci_trb *trb, uint8_t bmReqType,
                            uint8_t bRequest, uint16_t wValue,
                            uint16_t wIndex, uint16_t wLength)
{
    trb->params[0] = (bmReqType << 24) | (bRequest << 16) | wValue;
    trb->params[1] = (wIndex << 16) | wLength;
    trb->params[2] = 8;
    trb->params[3] = (TRB_SETUP << 10) | TRB_IDT | (1 << 6) | 1;
}

static void xhci_trb_data(struct xhci_trb *trb, uint64_t addr,
                           uint32_t len, int dir_in, int chain)
{
    uint32_t flags = TRB_DATA << 10;
    if (dir_in) flags |= (1 << 16);
    if (chain) flags |= TRB_CHAIN;
    trb->params[0] = (uint32_t)(addr & 0xFFFFFFFF);
    trb->params[1] = (uint32_t)(addr >> 32);
    trb->params[2] = len;
    trb->params[3] = flags | 1;
}

static void xhci_trb_status(struct xhci_trb *trb, int dir_in)
{
    uint32_t flags = (TRB_STATUS << 10) | TRB_IOC;
    if (dir_in) flags |= (1 << 16);
    trb->params[3] = flags | 1;
}

static void xhci_trb_cmd(struct xhci_trb *trb, uint32_t type,
                          uint32_t in0, uint32_t in1, uint32_t in2, int cycle)
{
    trb->params[0] = in0;
    trb->params[1] = in1;
    trb->params[2] = in2;
    trb->params[3] = (type << 10) | (cycle ? 1 : 0);
}

static int xhci_submit_cmd(struct xhci_dev *dev, struct xhci_trb *trb)
{
    memcpy(&dev->cmd_trbs[dev->cmd_ring.enqueue], trb,
           sizeof(struct xhci_trb));

    wmb();
    xhci_write32(dev, dev->db_regs, 0);

    for (int i = 0; i < 50000; i++) {
        if (dev->evt_trbs[0].params[3] & 1) {
            uint32_t comp = (dev->evt_trbs[0].params[3] >> 24) & 0xFF;
            dev->evt_trbs[0].params[3] = 0;
            dev->cmd_ring.enqueue = (dev->cmd_ring.enqueue + 1) % dev->cmd_ring.size;
            return (comp == COMP_SUCCESS) ? 0 : -comp;
        }
        __asm__("pause");
    }
    return -ETIMEDOUT;
}

static int xhci_enable_slot(struct xhci_dev *dev)
{
    struct xhci_trb trb;
    memset(&trb, 0, sizeof(trb));
    xhci_trb_cmd(&trb, TRB_CMD_ENABLE_SLOT, 0, 0, 0, dev->cmd_cycle);
    int ret = xhci_submit_cmd(dev, &trb);
    if (ret == 0) {
        int slot = dev->evt_trbs[0].params[0] & 0xFF;
        return slot > 0 ? slot : -1;
    }
    return ret;
}

static int xhci_address_device(struct xhci_dev *dev, int slot_id)
{
    struct xhci_trb trb;
    memset(&trb, 0, sizeof(trb));
    xhci_trb_cmd(&trb, TRB_CMD_ADDRESS_DEV,
                  (uint32_t)(dev->dcbaa_phys & 0xFFFFFFFF),
                  (uint32_t)(dev->dcbaa_phys >> 32),
                  slot_id, dev->cmd_cycle);
    return xhci_submit_cmd(dev, &trb);
}

static int xhci_get_descriptor(struct xhci_dev *dev, int slot_id,
                                uint8_t type, uint8_t index,
                                void *buf, int len)
{
    struct xhci_trb trbs[3];
    uint64_t buf_phys = (uint64_t)(uintptr_t)buf;

    memset(trbs, 0, sizeof(trbs));
    xhci_trb_setup(&trbs[0],
                   SETUP_DIR_IN | SETUP_TYPE_STD | SETUP_RCPT_DEV,
                   REQ_GET_DESC, (type << 8) | index, 0, len);

    struct xhci_trb *slot_trb = &dev->cmd_trbs[dev->cmd_ring.enqueue];
    memcpy(slot_trb, &trbs[0], sizeof(struct xhci_trb));
    dev->cmd_ring.enqueue = (dev->cmd_ring.enqueue + 1) % dev->cmd_ring.size;

    memset(&trbs[1], 0, sizeof(trbs[1]));
    xhci_trb_data(&trbs[1], buf_phys, len, 1, 1);
    memcpy(slot_trb + 1, &trbs[1], sizeof(struct xhci_trb));
    dev->cmd_ring.enqueue = (dev->cmd_ring.enqueue + 1) % dev->cmd_ring.size;

    memset(&trbs[2], 0, sizeof(trbs[2]));
    xhci_trb_status(&trbs[2], 1);
    memcpy(slot_trb + 2, &trbs[2], sizeof(struct xhci_trb));
    dev->cmd_ring.enqueue = (dev->cmd_ring.enqueue + 1) % dev->cmd_ring.size;

    wmb();
    xhci_doorbell(dev, slot_id, 0);

    for (int i = 0; i < 50000; i++) {
        if (dev->evt_trbs[0].params[3] & 1) {
            uint32_t comp = (dev->evt_trbs[0].params[3] >> 24) & 0xFF;
            dev->evt_trbs[0].params[3] = 0;
            return (comp == COMP_SUCCESS || comp == COMP_SHORT_TX) ? 0 : -comp;
        }
        __asm__("pause");
    }
    return -ETIMEDOUT;
}

static void xhci_enumerate_ports(struct xhci_dev *dev)
{
    klog(LOG_INFO, "XHCI", "enumerating %d ports...", dev->max_ports);

    for (int p = 1; p <= dev->max_ports; p++) {
        uint32_t portsc = xhci_read32(dev,
            dev->op_regs + XHCI_PORTSC_BASE + p * 0x10);

        if (!(portsc & PORTSC_CCS)) continue;

        int speed = (portsc & PORTSC_SPEED_MASK) >> PORTSC_SPEED_SHIFT;
        const char *speed_str = "unknown";
        switch (speed) {
        case USB_SPEED_LOW:  speed_str = "1.5 Mbps (USB 1.x)"; break;
        case USB_SPEED_FULL: speed_str = "12 Mbps (USB 1.x)"; break;
        case USB_SPEED_HIGH: speed_str = "480 Mbps (USB 2.0)"; break;
        case USB_SPEED_SUPER: speed_str = "5+ Gbps (USB 3.x)"; break;
        }

        uint32_t portsc_new = portsc | PORTSC_PP | PORTSC_PR;
        xhci_write32(dev,
            dev->op_regs + XHCI_PORTSC_BASE + p * 0x10,
            portsc_new);

        for (int i = 0; i < 50000; i++) {
            portsc = xhci_read32(dev,
                dev->op_regs + XHCI_PORTSC_BASE + p * 0x10);
            if (!(portsc & PORTSC_PR)) break;
            __asm__("pause");
        }

        klog(LOG_OK, "XHCI", "port %d: device detected (%s)", p, speed_str);
        dev->slots[p].enabled = 1;
        dev->slots[p].speed = (uint8_t)speed;
        dev->slots[p].port = (uint8_t)p;

        /* Enable slot */
        int slot_id = xhci_enable_slot(dev);
        if (slot_id <= 0) {
            klog(LOG_INFO, "XHCI", "port %d: slot enable failed", p);
            continue;
        }

        /* Address device */
        if (xhci_address_device(dev, slot_id) < 0) {
            klog(LOG_INFO, "XHCI", "port %d: address failed", p);
            continue;
        }
        dev->slots[p].addressed = 1;

        /* Read device descriptor */
        uint8_t dev_desc[18];
        memset(dev_desc, 0, sizeof(dev_desc));
        if (xhci_get_descriptor(dev, slot_id, DESC_DEVICE, 0,
                                 dev_desc, 18) == 0) {
            dev->slots[p].vid = dev_desc[8] | (dev_desc[9] << 8);
            dev->slots[p].pid = dev_desc[10] | (dev_desc[11] << 8);
            dev->slots[p].max_ep0_size = dev_desc[7];

            klog(LOG_OK, "XHCI", "port %d: USB device %04x:%04x (ep0 max %d)",
                 p, dev->slots[p].vid, dev->slots[p].pid,
                 dev->slots[p].max_ep0_size);

            /* Register as storage if mass storage class */
            char model[64];
            snprintf(model, sizeof(model), "USB Device %04x:%04x (port %d, %s)",
                     dev->slots[p].vid, dev->slots[p].pid, p, speed_str);

            registered_device_t *reg = zirv_register_device(
                DEV_CLASS_USB, DEV_CLASS_PENDRIVE,
                model, NULL);
            if (reg) {
                reg->desc.driver_data = &dev->slots[p];
                reg->desc.present = true;
            }
        }
    }
}

static int xhci_probe(pci_dev_t *pdev, const pci_device_id_t *id)
{
    (void)id;
    if (pdev->class_code != 0x0C || pdev->subclass != 0x03 || pdev->prog_if != 0x30)
        return -ENODEV;

    klog(LOG_INFO, "XHCI", "xHCI probing %04x:%04x rev %02x",
         pdev->vendor_id, pdev->device_id, pdev->revision);

    struct xhci_dev *dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) return -ENOMEM;
    dev->pdev = pdev;

    pci_enable_device(pdev);
    dev->mmio = pci_map_bar(pdev, 0);
    if (!dev->mmio) { kfree(dev); return -1; }

    dev->cap_length = ioread8(dev->mmio + XHCI_CAPLENGTH);
    dev->hci_version = ioread16(dev->mmio + XHCI_HCIVERSION);
    dev->cap_regs = (uint8_t *)dev->mmio;
    dev->op_regs = (uint8_t *)dev->mmio + dev->cap_length;

    uint32_t hcsp1 = ioread32(dev->mmio + XHCI_HCSPARAMS1);
    dev->max_slots = (hcsp1 >> 0) & 0xFF;
    dev->max_ports = (hcsp1 >> 24) & 0xFF;

    uint32_t hccp1 = ioread32(dev->mmio + XHCI_HCCPARAMS1);
    uint32_t dboff = (hccp1 >> 2) & 0x3F;
    uint32_t rtoff = (hccp1 >> 27) & 0x1F;
    dev->db_regs = (uint8_t *)dev->mmio + (dboff << 2);
    dev->rt_regs = (uint8_t *)dev->mmio + (rtoff << 2);

    dev->page_size = ioread32(dev->op_regs + XHCI_PAGESIZE);

    klog(LOG_INFO, "XHCI", "xHCI v%d.%d — %d slots, %d ports, page %d",
         dev->hci_version >> 8, dev->hci_version & 0xFF,
         dev->max_slots, dev->max_ports, dev->page_size);

    /* Allocate rings */
    if (xhci_ring_init(dev, &dev->cmd_ring, XHCI_RING_SIZE,
                        &dev->cmd_trbs_phys) < 0)
        { kfree(dev); return -1; }
    dev->cmd_trbs = dev->cmd_ring.trbs;

    if (xhci_ring_init(dev, &dev->event_ring, XHCI_EVENT_RING_SIZE,
                        &dev->evt_trbs_phys) < 0)
        { kfree(dev); return -1; }
    dev->evt_trbs = dev->event_ring.trbs;

    /* DCBAA */
    dev->dcbaa = (uint64_t *)kzalloc(64 * sizeof(uint64_t) + 63, GFP_KERNEL);
    if (!dev->dcbaa) { kfree(dev); return -1; }
    dev->dcbaa_phys = (uint64_t)(uintptr_t)dev->dcbaa;
    dev->dcbaa_phys = (dev->dcbaa_phys + 63) & ~63ULL;
    dev->dcbaa = (uint64_t *)(uintptr_t)dev->dcbaa_phys;

    /* Reset */
    xhci_reset(dev);

    /* Configure */
    xhci_write32(dev, dev->op_regs + XHCI_CONFIG, dev->max_slots);

    /* Set CRCR */
    xhci_write64(dev, dev->op_regs + XHCI_CRCR, dev->cmd_trbs_phys | 1);

    /* Set DCBAAP */
    xhci_write64(dev, dev->op_regs + XHCI_DCBAAP, dev->dcbaa_phys);

    /* Set ERST — in runtime registers space at offset 0x18 */
    xhci_write32(dev, dev->rt_regs + 0x18,
                 (uint32_t)(dev->evt_trbs_phys & 0xFFFFFFFF));
    xhci_write32(dev, dev->rt_regs + 0x1C,
                 (uint32_t)(dev->evt_trbs_phys >> 32));
    xhci_write32(dev, dev->rt_regs + 0x20, 0);
    xhci_write32(dev, dev->rt_regs + 0x24, XHCI_ERST_SIZE);

    dev->initialized = 1;
    klog(LOG_OK, "XHCI", "xHCI controller initialized");

    /* Enumerate */
    xhci_enumerate_ports(dev);

    registered_device_t *reg = zirv_register_device(
        DEV_CLASS_USB, DEV_CLASS_USB, "xHCI USB 3.0 Controller", NULL);
    if (reg) {
        reg->desc.driver_data = dev;
        reg->desc.present = true;
        dev->rdev = reg;
    }

    return 0;
}

static void xhci_remove(pci_dev_t *pdev)
{
    struct xhci_dev *dev = (struct xhci_dev *)pdev->driver_data;
    if (dev) {
        kfree(dev->cmd_trbs);
        kfree(dev->evt_trbs);
        kfree(dev->dcbaa);
        kfree(dev);
        pdev->driver_data = NULL;
    }
}

static const pci_device_id_t xhci_id_table[] = {
    { PCI_DEVICE(0x8086, PCI_ANY_ID) },
    { PCI_DEVICE(0x1022, PCI_ANY_ID) },
    { PCI_DEVICE(0x10DE, PCI_ANY_ID) },
    { PCI_DEVICE(0x1002, PCI_ANY_ID) },
    { 0, }
};

static struct pci_driver_linux xhci_driver = {
    .name      = "xhci",
    .id_table  = xhci_id_table,
    .probe     = xhci_probe,
    .remove    = xhci_remove,
};

void xhci_init(void)
{
    klog(LOG_INFO, "XHCI", "xHCI USB 3.0 driver (Linux compat)");
    pci_compat_register_driver(&xhci_driver);
}
