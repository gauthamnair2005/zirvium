/* drivers/pci/pci.h
 * Zirvium Kernel — PCI subsystem
 *
 * Provides PCI device enumeration, configuration-space access, BAR mapping,
 * and driver registration.  Used by i915, RTL8723DE, and NVMe drivers.
 */
#ifndef ZIRVIUM_DRIVERS_PCI_PCI_H
#define ZIRVIUM_DRIVERS_PCI_PCI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── PCI configuration-space register offsets ────────────────────────────── */
#define PCI_VENDOR_ID           0x00
#define PCI_DEVICE_ID           0x02
#define PCI_COMMAND             0x04
#define PCI_STATUS              0x06
#define PCI_REVISION_ID         0x08
#define PCI_PROG_IF             0x09
#define PCI_SUBCLASS            0x0A
#define PCI_CLASS               0x0B
#define PCI_CACHE_LINE_SIZE     0x0C
#define PCI_LATENCY_TIMER       0x0D
#define PCI_HEADER_TYPE         0x0E
#define PCI_BIST                0x0F
#define PCI_BAR0                0x10
#define PCI_BAR1                0x14
#define PCI_BAR2                0x18
#define PCI_BAR3                0x1C
#define PCI_BAR4                0x20
#define PCI_BAR5                0x24
#define PCI_SUBSYSTEM_VENDOR    0x2C
#define PCI_SUBSYSTEM_ID        0x2E
#define PCI_ROM_ADDRESS         0x30
#define PCI_CAPABILITIES_PTR    0x34
#define PCI_INTERRUPT_LINE      0x3C
#define PCI_INTERRUPT_PIN       0x3D

/* PCI Command register bits */
#define PCI_CMD_IO_SPACE        (1 << 0)
#define PCI_CMD_MEM_SPACE       (1 << 1)
#define PCI_CMD_BUS_MASTER      (1 << 2)
#define PCI_CMD_INT_DISABLE     (1 << 10)

/* PCI BAR flags */
#define PCI_BAR_IO              (1 << 0)
#define PCI_BAR_MEM64           (2 << 1)
#define PCI_BAR_PREFETCH        (1 << 3)

/* PCI capability IDs */
#define PCI_CAP_ID_MSI          0x05
#define PCI_CAP_ID_MSIX         0x11
#define PCI_CAP_ID_EXP          0x10   /* PCIe capability */

/* PCI class codes */
#define PCI_CLASS_DISPLAY       0x03
#define PCI_CLASS_NETWORK       0x02
#define PCI_CLASS_STORAGE       0x01
#define PCI_CLASS_BRIDGE        0x06

/* ── Maximum devices tracked ─────────────────────────────────────────────── */
#define PCI_MAX_DEVICES  128
#define PCI_MAX_BARS       6

/* ── PCI BAR descriptor ───────────────────────────────────────────────────── */
typedef struct {
    uint64_t phys_addr;
    uint64_t size;
    bool     is_io;
    bool     is_64bit;
    bool     prefetchable;
    void    *virt_addr;    /* mapped virtual address, NULL if not mapped */
} pci_bar_t;

/* ── PCI device descriptor ────────────────────────────────────────────────── */
typedef struct pci_dev {
    uint8_t  bus, dev, func;
    uint16_t vendor_id, device_id;
    uint16_t subsys_vendor, subsys_device;
    uint8_t  class_code, subclass, prog_if, revision;
    uint8_t  header_type;
    uint8_t  irq_line, irq_pin;
    pci_bar_t bars[PCI_MAX_BARS];
    void    *driver_data;         /* driver-private storage */
    const char *driver_name;      /* name of driver that claimed this device */
} pci_dev_t;

typedef struct {
    uint16_t vendor_id, device_id;
    uint16_t subsys_vendor, subsys_device;
    uint8_t  bus, dev, func;
    uint8_t  class_code, subclass, prog_if, revision;
    uint8_t  irq_line;
    uint8_t  _pad;
    uint64_t bar0_addr, bar0_size;
    uint64_t bar2_addr, bar2_size;
    char     driver_name[32];
} pci_dev_info_t;

/* ── PCI driver registration ──────────────────────────────────────────────── */
typedef struct {
    uint16_t vendor, device;
    uint16_t subvendor, subdevice;   /* 0xFFFF = wildcard */
    uint8_t  class_code, subclass;   /* 0xFF = wildcard */
} pci_id_t;

#define PCI_ID_ANY  0xFFFF
#define PCI_CLASS_ANY 0xFF

typedef struct pci_driver {
    const char   *name;
    const pci_id_t *id_table;
    size_t        id_count;
    int  (*probe)(pci_dev_t *pdev, const pci_id_t *id);
    void (*remove)(pci_dev_t *pdev);
} pci_driver_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/** pci_init - scan all PCI buses and build the device list. */
void pci_init(void);

/** pci_device_count - return total number of discovered PCI devices. */
size_t pci_device_count(void);

/** pci_get_device - get device by index (0 .. pci_device_count()-1). */
pci_dev_t *pci_get_device(size_t index);

/** pci_find_device - find first device matching vendor:device IDs.
 *  Pass PCI_ID_ANY (0xFFFF) as a wildcard. */
pci_dev_t *pci_find_device(uint16_t vendor, uint16_t device);

/** pci_find_device_by_class - find first device matching class:subclass.
 *  Pass PCI_CLASS_ANY (0xFF) as wildcard. */
pci_dev_t *pci_find_device_by_class(uint8_t class_code, uint8_t subclass);

/** pci_read_config8/16/32 - raw configuration-space read. */
uint8_t  pci_read_config8 (pci_dev_t *dev, uint8_t offset);
uint16_t pci_read_config16(pci_dev_t *dev, uint8_t offset);
uint32_t pci_read_config32(pci_dev_t *dev, uint8_t offset);

/** pci_write_config8/16/32 - raw configuration-space write. */
void pci_write_config8 (pci_dev_t *dev, uint8_t offset, uint8_t  val);
void pci_write_config16(pci_dev_t *dev, uint8_t offset, uint16_t val);
void pci_write_config32(pci_dev_t *dev, uint8_t offset, uint32_t val);

/** pci_enable_device - enable memory/IO space and bus mastering. */
void pci_enable_device(pci_dev_t *dev);

/** pci_map_bar - map BAR @bar_idx into virtual memory.
 *  Returns the virtual address, or NULL on failure. */
void *pci_map_bar(pci_dev_t *dev, int bar_idx);

/** pci_register_driver - probe all matching devices with @drv. */
int pci_register_driver(pci_driver_t *drv);

#endif /* ZIRVIUM_DRIVERS_PCI_PCI_H */
