#include "kernel/net/e1000_regs.h"
#include "kernel/net/stack.h"
#include "drivers/zirv/driver.h"
#include "drivers/zirv/device.h"
#include "drivers/pci/pci.h"
#include "kernel/console.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "fs/mosix.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

extern void *kmalloc(size_t size, unsigned int flags);

/* ── Driver-private state ────────────────────────────────────────────────── */
typedef struct e1000_priv {
    uint8_t           mac[6];
    volatile void    *mmio;
    pci_dev_t        *pdev;
    e1000_rx_desc_t  *rx_ring;
    uint64_t          rx_ring_phys;
    uint8_t         **rx_bufs;
    uint32_t          rx_cur;
    e1000_tx_desc_t  *tx_ring;
    uint64_t          tx_ring_phys;
    uint8_t         **tx_bufs;
    uint32_t          tx_cur;
} e1000_priv_t;

static e1000_priv_t g_e1000;

/* ── MMIO accessors ──────────────────────────────────────────────────────── */
static inline uint32_t e1000_read32(uint32_t reg)
{
    return *(volatile uint32_t *)((uintptr_t)g_e1000.mmio + reg);
}
static inline void e1000_write32(uint32_t reg, uint32_t val)
{
    *(volatile uint32_t *)((uintptr_t)g_e1000.mmio + reg) = val;
}

/* ── Read MAC address ────────────────────────────────────────────────────── */
static void e1000_read_mac(void)
{
    uint32_t lo = e1000_read32(E1000_RA);
    uint32_t hi = e1000_read32(E1000_RA2);
    if ((lo | hi) != 0 && (lo | hi) != 0xFFFFFFFF) {
        g_e1000.mac[0] =  lo        & 0xFF;
        g_e1000.mac[1] = (lo >> 8)  & 0xFF;
        g_e1000.mac[2] = (lo >> 16) & 0xFF;
        g_e1000.mac[3] = (lo >> 24) & 0xFF;
        g_e1000.mac[4] =  hi        & 0xFF;
        g_e1000.mac[5] = (hi >> 8)  & 0xFF;
        return;
    }
    for (int i = 0; i < 3; i++) {
        e1000_write32(E1000_EERD, (uint32_t)i << 8 | 1);
        uint32_t v = 0;
        for (int t = 0; t < 1000; t++) { v = e1000_read32(E1000_EERD); if (v & 0x10) break; }
        uint16_t w = (uint16_t)(v >> 16);
        g_e1000.mac[i*2]     = w & 0xFF;
        g_e1000.mac[i*2 + 1] = w >> 8;
    }
}

/* ── Hardware initialisation ─────────────────────────────────────────────── */
static int e1000_hw_init(void)
{
    e1000_write32(E1000_CTRL, E1000_CTRL_RST);
    for (int i = 0; i < 10000; i++) { __asm__("pause"); if (!(e1000_read32(E1000_CTRL) & E1000_CTRL_RST)) break; }

    e1000_write32(E1000_CTRL, e1000_read32(E1000_CTRL) | E1000_CTRL_SLU);
    for (int i = 0; i < 100000; i++) { if (e1000_read32(E1000_STATUS) & 2) break; __asm__("pause"); }

    e1000_write32(E1000_RA,
        (uint32_t)g_e1000.mac[0] | (uint32_t)g_e1000.mac[1] << 8 |
        (uint32_t)g_e1000.mac[2] << 16 | (uint32_t)g_e1000.mac[3] << 24);
    e1000_write32(E1000_RA2,
        (uint32_t)g_e1000.mac[4] | (uint32_t)g_e1000.mac[5] << 8 | (1u << 31));

    g_e1000.rx_ring_phys = pmm_alloc_pages(1);
    if (!g_e1000.rx_ring_phys) return -1;
    g_e1000.rx_ring = (e1000_rx_desc_t *)PHYS_TO_VIRT(g_e1000.rx_ring_phys);
    memset(g_e1000.rx_ring, 0, 4096);
    g_e1000.rx_bufs = (uint8_t **)kmalloc(E1000_NUM_RX_DESC * sizeof(uint8_t *), 0);
    if (!g_e1000.rx_bufs) return -1;
    for (uint32_t i = 0; i < E1000_NUM_RX_DESC; i++) {
        uint64_t bp = pmm_alloc_page();
        if (!bp) return -1;
        g_e1000.rx_bufs[i] = (uint8_t *)PHYS_TO_VIRT(bp);
        g_e1000.rx_ring[i].addr = bp;
    }
    g_e1000.rx_cur = 0;
    e1000_write32(E1000_RDBAL, (uint32_t)g_e1000.rx_ring_phys);
    e1000_write32(E1000_RDBAH, (uint32_t)(g_e1000.rx_ring_phys >> 32));
    e1000_write32(E1000_RDLEN, E1000_NUM_RX_DESC * (uint32_t)sizeof(e1000_rx_desc_t));
    e1000_write32(E1000_RDH, 0);
    e1000_write32(E1000_RDT, E1000_NUM_RX_DESC - 1);
    e1000_write32(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_UPE |
                              E1000_RCTL_MPE | E1000_RCTL_SECRC);

    g_e1000.tx_ring_phys = pmm_alloc_pages(1);
    if (!g_e1000.tx_ring_phys) return -1;
    g_e1000.tx_ring = (e1000_tx_desc_t *)PHYS_TO_VIRT(g_e1000.tx_ring_phys);
    memset(g_e1000.tx_ring, 0, 4096);
    g_e1000.tx_bufs = (uint8_t **)kmalloc(E1000_NUM_TX_DESC * sizeof(uint8_t *), 0);
    if (!g_e1000.tx_bufs) return -1;
    for (uint32_t i = 0; i < E1000_NUM_TX_DESC; i++) {
        uint64_t bp = pmm_alloc_page();
        if (!bp) return -1;
        g_e1000.tx_bufs[i] = (uint8_t *)PHYS_TO_VIRT(bp);
        g_e1000.tx_ring[i].addr = bp;
    }
    g_e1000.tx_cur = 0;
    e1000_write32(E1000_TDBAL, (uint32_t)g_e1000.tx_ring_phys);
    e1000_write32(E1000_TDBAH, (uint32_t)(g_e1000.tx_ring_phys >> 32));
    e1000_write32(E1000_TDLEN, E1000_NUM_TX_DESC * (uint32_t)sizeof(e1000_tx_desc_t));
    e1000_write32(E1000_TDH, 0);
    e1000_write32(E1000_TDT, 0);
    e1000_write32(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << 4) | (0x40 << 12));
    e1000_write32(E1000_TIPG, 0x0060200A);
    e1000_write32(E1000_IMS, 0);
    return 0;
}

/* ── Send raw frame ──────────────────────────────────────────────────────── */
static int e1000_send(const void *data, uint16_t len)
{
    if (len > 2048) return -1;
    uint32_t td = g_e1000.tx_cur;
    if (g_e1000.tx_ring[td].status & 1) return -1;
    memcpy(g_e1000.tx_bufs[td], data, len);
    g_e1000.tx_ring[td].length = len;
    g_e1000.tx_ring[td].cmd    = 0x0B;
    g_e1000.tx_ring[td].status = 0;
    g_e1000.tx_cur = (g_e1000.tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write32(E1000_TDT, g_e1000.tx_cur);
    for (int i = 0; i < 100000; i++) { if (g_e1000.tx_ring[td].status & 1) break; __asm__("pause"); }
    return (int)len;
}

/* ── Poll for one received frame (non-blocking, returns 0 if none) ──────── */
static int e1000_poll_one(uint8_t *buf, uint16_t buflen)
{
    uint32_t rd = (g_e1000.rx_cur + 1) % E1000_NUM_RX_DESC;
    e1000_rx_desc_t *d = &g_e1000.rx_ring[rd];
    if (!(d->status & 1)) return 0;
    uint16_t plen = d->length;
    if (plen > buflen) plen = buflen;
    memcpy(buf, g_e1000.rx_bufs[rd], plen);
    d->status = 0;
    g_e1000.rx_cur = rd;
    e1000_write32(E1000_RDT, g_e1000.rx_cur);
    return (int)plen;
}

/* ── VFAT write: user-space writes raw frames ────────────────────────────── */
static int e1000_vfs_write(vnode_t *vn, const void *buf, size_t count, uint64_t off)
{
    (void)vn; (void)off;
    if (!g_e1000.mmio || count == 0 || count > 2048) return -1;
    return e1000_send(buf, (uint16_t)count);
}

/* ── VFAT read: returns next user-space frame (handles ARP in-kernel) ────── */
static int e1000_vfs_read(vnode_t *vn, void *buf, size_t count, uint64_t off)
{
    (void)vn; (void)off;
    if (!g_e1000.mmio || count == 0) return -1;
    for (;;) {
        uint8_t frame[2048];
        int n = e1000_poll_one(frame, sizeof(frame));
        if (n <= 0) { __asm__("pause"); continue; }
        if (net_stack_rx(frame, (uint16_t)n)) continue;
        if ((size_t)n > count) n = (int)count;
        memcpy(buf, frame, (size_t)n);
        return n;
    }
}

static const vnode_ops_t e1000_vnode_ops = {
    .read  = e1000_vfs_read,
    .write = e1000_vfs_write,
};

/* ── PCI probe ───────────────────────────────────────────────────────────── */
static int intel_e1000_probe(void *hw_desc)
{
    pci_dev_t *pdev = (pci_dev_t *)hw_desc;
    memset(&g_e1000, 0, sizeof(g_e1000));
    g_e1000.pdev = pdev;
    pci_enable_device(pdev);
    g_e1000.mmio = pci_map_bar(pdev, 0);
    if (!g_e1000.mmio) { klog(LOG_FAIL, "NET ", "e1000: BAR0 failed"); return -1; }
    e1000_read_mac();
    klog(LOG_INFO, "NET ", "e1000: MAC %02x:%02x:%02x:%02x:%02x:%02x",
         g_e1000.mac[0], g_e1000.mac[1], g_e1000.mac[2],
         g_e1000.mac[3], g_e1000.mac[4], g_e1000.mac[5]);
    if (e1000_hw_init() < 0) { klog(LOG_FAIL, "NET ", "e1000: HW init failed"); return -1; }
    klog(LOG_OK, "NET ", "e1000: Link up, ready");

    registered_device_t *reg = zirv_register_device(
        DEV_CLASS_NET_ETH, DEV_CLASS_NET_ETH, "Intel 82540EM", NULL);
    if (!reg || !reg->vnode) { klog(LOG_FAIL, "NET ", "e1000: reg failed"); return -1; }
    reg->vnode->ops = &e1000_vnode_ops;

    net_stack_init(g_e1000.mac);
    net_stack_set_send(e1000_send);
    return 0;
}

const zirv_driver_t g_intel_e1000_driver = {
    .name = "Intel E1000",
    .component_tag = "NET ",
    .type = DRIVER_TYPE_PCI,
    .match.pci = { .vendor_id = 0x8086, .device_id = 0x100E },
    .probe = intel_e1000_probe,
};
