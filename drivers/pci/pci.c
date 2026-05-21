/* drivers/pci/pci.c
 * Zirvium Kernel — PCI subsystem implementation
 *
 * Performs a brute-force enumeration of all PCI buses (0-255), devices
 * (0-31) and functions (0-7) using the legacy CF8/CFC I/O mechanism.
 * For each discovered device it reads the standard config header, decodes
 * all six BARs, and stores the result in a flat device list.
 */
#include "pci.h"
#include "arch/x64/cpu.h"
#include "kernel/mm/vmm.h"   /* PHYS_TO_VIRT, PAGE_SIZE */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── CF8/CFC port I/O ─────────────────────────────────────────────────────── */
#define PCI_ADDR  0xCF8
#define PCI_DATA  0xCFC

static inline uint32_t pci_cfg_addr(uint8_t bus, uint8_t dev,
                                     uint8_t func, uint8_t off)
{
    return (1U << 31)
         | ((uint32_t)bus  << 16)
         | ((uint32_t)dev  << 11)
         | ((uint32_t)func <<  8)
         | (off & 0xFC);
}

static uint32_t cfg_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off)
{
    outl(PCI_ADDR, pci_cfg_addr(bus, dev, func, off));
    return inl(PCI_DATA);
}
static void cfg_write32(uint8_t bus, uint8_t dev, uint8_t func,
                         uint8_t off, uint32_t val)
{
    outl(PCI_ADDR, pci_cfg_addr(bus, dev, func, off));
    outl(PCI_DATA, val);
}

/* ── Device table ─────────────────────────────────────────────────────────── */
static pci_dev_t pci_devices[PCI_MAX_DEVICES];
static size_t    pci_dev_count = 0;

/* ── BAR decoding ─────────────────────────────────────────────────────────── */
static void decode_bars(pci_dev_t *d)
{
    for (int i = 0; i < PCI_MAX_BARS; ) {
        uint8_t  off = (uint8_t)(PCI_BAR0 + i * 4);
        uint32_t bar = cfg_read32(d->bus, d->dev, d->func, off);

        if (bar == 0) { i++; continue; }

        pci_bar_t *b = &d->bars[i];

        if (bar & PCI_BAR_IO) {
            /* I/O space BAR */
            b->is_io    = true;
            b->phys_addr = bar & ~0x3U;
            /* Determine size: write all-1s, read back, restore */
            cfg_write32(d->bus, d->dev, d->func, off, 0xFFFFFFFF);
            uint32_t sz = cfg_read32(d->bus, d->dev, d->func, off);
            cfg_write32(d->bus, d->dev, d->func, off, bar);
            b->size = (~(sz & ~0x3U) + 1) & 0xFFFF;
            i++;
        } else {
            /* Memory space BAR */
            bool is64 = ((bar >> 1) & 0x3) == 2;
            b->prefetchable = (bar & PCI_BAR_PREFETCH) != 0;
            b->is_64bit     = is64;
            b->phys_addr    = bar & ~0xFU;

            if (is64 && i + 1 < PCI_MAX_BARS) {
                uint32_t bar_hi = cfg_read32(d->bus, d->dev, d->func,
                                             (uint8_t)(off + 4));
                b->phys_addr |= ((uint64_t)bar_hi << 32);
            }

            /* Determine size */
            cfg_write32(d->bus, d->dev, d->func, off, 0xFFFFFFFF);
            uint32_t sz_lo = cfg_read32(d->bus, d->dev, d->func, off);
            cfg_write32(d->bus, d->dev, d->func, off, bar);

            uint64_t sz = ~(uint64_t)(sz_lo & ~0xFU) + 1;
            if (is64) sz &= 0xFFFFFFFF;   /* 32-bit size field */
            b->size = sz;

            i += is64 ? 2 : 1;
        }
    }
}

/* ── Enumerate one bus:dev:func ───────────────────────────────────────────── */
static void probe_function(uint8_t bus, uint8_t dev, uint8_t func)
{
    uint32_t id = cfg_read32(bus, dev, func, PCI_VENDOR_ID);
    if ((id & 0xFFFF) == 0xFFFF) return;   /* not present */
    if (pci_dev_count >= PCI_MAX_DEVICES) return;

    pci_dev_t *d = &pci_devices[pci_dev_count++];
    d->bus    = bus;
    d->dev    = dev;
    d->func   = func;
    d->vendor_id = (uint16_t)(id & 0xFFFF);
    d->device_id = (uint16_t)((id >> 16) & 0xFFFF);

    uint32_t class_rev = cfg_read32(bus, dev, func, PCI_REVISION_ID);
    d->revision  = (uint8_t)(class_rev & 0xFF);
    d->prog_if   = (uint8_t)((class_rev >>  8) & 0xFF);
    d->subclass  = (uint8_t)((class_rev >> 16) & 0xFF);
    d->class_code= (uint8_t)((class_rev >> 24) & 0xFF);

    uint32_t hdr_bist = cfg_read32(bus, dev, func, PCI_CACHE_LINE_SIZE);
    d->header_type = (uint8_t)((hdr_bist >> 16) & 0x7F);

    uint32_t ss = cfg_read32(bus, dev, func, PCI_SUBSYSTEM_VENDOR);
    d->subsys_vendor = (uint16_t)(ss & 0xFFFF);
    d->subsys_device = (uint16_t)((ss >> 16) & 0xFFFF);

    uint32_t irq = cfg_read32(bus, dev, func, PCI_INTERRUPT_LINE);
    d->irq_line = (uint8_t)(irq & 0xFF);
    d->irq_pin  = (uint8_t)((irq >> 8) & 0xFF);

    /* Only decode BARs for endpoint devices (header type 0) */
    if (d->header_type == 0)
        decode_bars(d);
}

/* ── Public API ───────────────────────────────────────────────────────────── */
void pci_init(void)
{
    pci_dev_count = 0;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id = cfg_read32((uint8_t)bus, dev, 0, PCI_VENDOR_ID);
            if ((id & 0xFFFF) == 0xFFFF) continue;

            uint32_t hdr = cfg_read32((uint8_t)bus, dev, 0,
                                       PCI_CACHE_LINE_SIZE);
            bool multi = ((hdr >> 16) & 0x80) != 0;
            uint8_t nfunc = multi ? 8 : 1;

            for (uint8_t func = 0; func < nfunc; func++)
                probe_function((uint8_t)bus, dev, func);
        }
    }
}

size_t pci_device_count(void) { return pci_dev_count; }

pci_dev_t *pci_get_device(size_t index)
{
    if (index >= pci_dev_count) return NULL;
    return &pci_devices[index];
}

pci_dev_t *pci_find_device(uint16_t vendor, uint16_t device)
{
    for (size_t i = 0; i < pci_dev_count; i++) {
        pci_dev_t *d = &pci_devices[i];
        if ((vendor == PCI_ID_ANY || d->vendor_id == vendor) &&
            (device == PCI_ID_ANY || d->device_id == device))
            return d;
    }
    return NULL;
}

pci_dev_t *pci_find_device_by_class(uint8_t class_code, uint8_t subclass)
{
    for (size_t i = 0; i < pci_dev_count; i++) {
        pci_dev_t *d = &pci_devices[i];
        if ((class_code == PCI_CLASS_ANY || d->class_code == class_code) &&
            (subclass   == PCI_CLASS_ANY || d->subclass   == subclass))
            return d;
    }
    return NULL;
}

uint8_t pci_read_config8(pci_dev_t *d, uint8_t off)
{
    return (uint8_t)(cfg_read32(d->bus, d->dev, d->func, off & ~3U)
                     >> ((off & 3) * 8));
}
uint16_t pci_read_config16(pci_dev_t *d, uint8_t off)
{
    return (uint16_t)(cfg_read32(d->bus, d->dev, d->func, off & ~3U)
                      >> ((off & 2) * 8));
}
uint32_t pci_read_config32(pci_dev_t *d, uint8_t off)
{
    return cfg_read32(d->bus, d->dev, d->func, off);
}
void pci_write_config8(pci_dev_t *d, uint8_t off, uint8_t val)
{
    uint32_t r = cfg_read32(d->bus, d->dev, d->func, off & ~3U);
    uint32_t shift = (off & 3) * 8;
    r = (r & ~(0xFFU << shift)) | ((uint32_t)val << shift);
    cfg_write32(d->bus, d->dev, d->func, off & ~3U, r);
}
void pci_write_config16(pci_dev_t *d, uint8_t off, uint16_t val)
{
    uint32_t r = cfg_read32(d->bus, d->dev, d->func, off & ~3U);
    uint32_t shift = (off & 2) * 8;
    r = (r & ~(0xFFFFU << shift)) | ((uint32_t)val << shift);
    cfg_write32(d->bus, d->dev, d->func, off & ~3U, r);
}
void pci_write_config32(pci_dev_t *d, uint8_t off, uint32_t val)
{
    cfg_write32(d->bus, d->dev, d->func, off, val);
}

void pci_enable_device(pci_dev_t *dev)
{
    uint16_t cmd = pci_read_config16(dev, PCI_COMMAND);
    cmd |= PCI_CMD_MEM_SPACE | PCI_CMD_BUS_MASTER;
    cmd &= ~(uint16_t)PCI_CMD_INT_DISABLE;
    pci_write_config16(dev, PCI_COMMAND, cmd);
}

void *pci_map_bar(pci_dev_t *dev, int bar_idx)
{
    if (bar_idx < 0 || bar_idx >= PCI_MAX_BARS) return NULL;
    pci_bar_t *bar = &dev->bars[bar_idx];
    if (bar->is_io || bar->phys_addr == 0) return NULL;
    if (bar->virt_addr) return bar->virt_addr;

    /* For addresses in the first 4 GiB use the VMM direct-map */
    bar->virt_addr = PHYS_TO_VIRT(bar->phys_addr);
    return bar->virt_addr;
}

int pci_register_driver(pci_driver_t *drv)
{
    int matched = 0;
    for (size_t di = 0; di < pci_dev_count; di++) {
        pci_dev_t *dev = &pci_devices[di];
        for (size_t ii = 0; ii < drv->id_count; ii++) {
            const pci_id_t *id = &drv->id_table[ii];
            if ((id->vendor  == PCI_ID_ANY || id->vendor  == dev->vendor_id) &&
                (id->device  == PCI_ID_ANY || id->device  == dev->device_id)) {
                if (drv->probe(dev, id) == 0) {
                    dev->driver_name = drv->name;
                    matched++;
                }
            }
        }
    }
    return matched;
}
