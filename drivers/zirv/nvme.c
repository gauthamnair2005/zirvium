/* drivers/zirv/nvme.c
 * Zirvium Kernel — NVMe PCIe SSD driver (polling mode, minimal)
 *
 * NVMe devices are accessed via MMIO.  The controller base address is read
 * from the PCI BAR0 of any PCI device with class 0x01 / subclass 0x08
 * (Non-Volatile Memory controller, NVM Express interface).
 *
 * Currently this file implements the structure and PCI scan; full I/O
 * queue command submission is left for the next driver iteration.
 */
#include "nvme.h"
#include "device.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── PCI config-space access (legacy CF8/CFC mechanism) ──────────────────── */
#include "arch/x64/cpu.h"

#define PCI_ADDR_PORT  0xCF8
#define PCI_DATA_PORT  0xCFC

static uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off)
{
    uint32_t addr = (1U << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)dev  << 11)
                  | ((uint32_t)func <<  8)
                  | (off & 0xFC);
    outl(PCI_ADDR_PORT, addr);
    return inl(PCI_DATA_PORT);
}

static void pci_write32(uint8_t bus, uint8_t dev, uint8_t func,
                         uint8_t off, uint32_t val)
{
    uint32_t addr = (1U << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)dev  << 11)
                  | ((uint32_t)func <<  8)
                  | (off & 0xFC);
    outl(PCI_ADDR_PORT, addr);
    outl(PCI_DATA_PORT, val);
}

/* ── NVMe Controller Registers (offset from BAR0 MMIO base) ─────────────── */
#define NVME_REG_CAP    0x00   /* Controller Capabilities (8 bytes) */
#define NVME_REG_VS     0x08   /* Version (4 bytes) */
#define NVME_REG_CC     0x14   /* Controller Configuration (4 bytes) */
#define NVME_REG_CSTS   0x1C   /* Controller Status (4 bytes) */
#define NVME_REG_AQA    0x24   /* Admin Queue Attributes (4 bytes) */
#define NVME_REG_ASQ    0x28   /* Admin Submission Queue Base (8 bytes) */
#define NVME_REG_ACQ    0x30   /* Admin Completion Queue Base (8 bytes) */

#define NVME_CSTS_RDY   (1U << 0)
#define NVME_CC_EN      (1U << 0)

/* ── NVMe driver ops ─────────────────────────────────────────────────────── */
static int nvme_dev_init(device_desc_t *dev)
{
    (void)dev;
    return 0;   /* Placeholder — full init done during PCI probe */
}

static int nvme_read_sectors(device_desc_t *dev, uint64_t lba,
                              uint32_t count, void *buf)
{
    /* TODO: submit NVM Read command via I/O submission queue */
    (void)dev; (void)lba; (void)count; (void)buf;
    return -1;   /* not yet implemented */
}

static int nvme_write_sectors(device_desc_t *dev, uint64_t lba,
                               uint32_t count, const void *buf)
{
    /* TODO: submit NVM Write command via I/O submission queue */
    (void)dev; (void)lba; (void)count; (void)buf;
    return -1;
}

static const dev_ops_t nvme_ops = {
    .init          = nvme_dev_init,
    .read_sectors  = nvme_read_sectors,
    .write_sectors = nvme_write_sectors,
    .ioctl         = NULL,
    .shutdown      = NULL,
};

/* ── PCI scan for NVMe controllers ──────────────────────────────────────── */
#define PCI_CLASS_STORAGE        0x01
#define PCI_SUBCLASS_NVME        0x08
#define PCI_PROGIF_NVME          0x02

static uint32_t nvme_count = 0;

static void probe_nvme_device(uint8_t bus, uint8_t dev, uint8_t func)
{
    /* Read class / subclass / prog-if */
    uint32_t class_rev = pci_read32(bus, dev, func, 0x08);
    uint8_t  bclass  = (uint8_t)((class_rev >> 24) & 0xFF);
    uint8_t  subclass = (uint8_t)((class_rev >> 16) & 0xFF);
    uint8_t  progif   = (uint8_t)((class_rev >>  8) & 0xFF);

    if (bclass != PCI_CLASS_STORAGE   ||
        subclass != PCI_SUBCLASS_NVME ||
        progif   != PCI_PROGIF_NVME)
        return;

    /* Read BAR0 (MMIO base) */
    uint32_t bar0_lo = pci_read32(bus, dev, func, 0x10) & ~0xFU;
    uint32_t bar0_hi = pci_read32(bus, dev, func, 0x14);
    uint64_t bar0    = ((uint64_t)bar0_hi << 32) | bar0_lo;

    if (!bar0) return;   /* BAR not configured */

    /* Enable bus-mastering and memory space decoding */
    uint32_t cmd = pci_read32(bus, dev, func, 0x04);
    pci_write32(bus, dev, func, 0x04, cmd | 0x06);

    /* Build a model string */
    char model[64];
    uint32_t vendor_dev = pci_read32(bus, dev, func, 0x00);
    uint16_t vendor_id  = (uint16_t)(vendor_dev & 0xFFFF);
    uint16_t device_id  = (uint16_t)((vendor_dev >> 16) & 0xFFFF);
    (void)vendor_id; (void)device_id;

    /* Simple model string with bus/device/function */
    const char *prefix = "NVMe SSD @ PCI ";
    size_t pl = strlen(prefix);
    memcpy(model, prefix, pl);
    /* Append bus:dev.func as hex digits */
    model[pl++] = "0123456789ABCDEF"[bus >> 4];
    model[pl++] = "0123456789ABCDEF"[bus & 0xF];
    model[pl++] = ':';
    model[pl++] = "0123456789ABCDEF"[dev >> 1];
    model[pl++] = "0123456789ABCDEF"[(dev & 1) << 3 | (func & 7)];
    model[pl]   = '\0';

    /* Register with the device registry */
    registered_device_t *reg =
        zirv_register_device(DEV_CLASS_NVME, DEV_CLASS_SSD, model, &nvme_ops);
    if (reg) {
        /* Store BAR0 in driver_data for later use */
        reg->desc.driver_data = (void *)(uintptr_t)bar0;
    }
    nvme_count++;
}

void nvme_init(void)
{
    /* Brute-force PCI bus scan (buses 0–255, devices 0–31, functions 0–7) */
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev_no = 0; dev_no < 32; dev_no++) {
            /* Check if device exists (vendor ID != 0xFFFF) */
            uint32_t id = pci_read32((uint8_t)bus, dev_no, 0, 0x00);
            if ((id & 0xFFFF) == 0xFFFF) continue;

            /* Check number of functions */
            uint32_t hdr = pci_read32((uint8_t)bus, dev_no, 0, 0x0C);
            uint8_t  htype = (uint8_t)((hdr >> 16) & 0xFF);
            uint8_t  nfunc = (htype & 0x80) ? 8 : 1;

            for (uint8_t func = 0; func < nfunc; func++)
                probe_nvme_device((uint8_t)bus, dev_no, func);
        }
    }
}
