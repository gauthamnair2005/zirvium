#ifndef _KERNEL_PCI_H
#define _KERNEL_PCI_H

#include <kernel/types.h>

struct pci_dev {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    void *driver_data;
};

struct pci_driver {
    const char *name;
    const struct pci_device_id *id_table;
    int (*probe)(struct pci_dev *dev);
    void (*remove)(struct pci_dev *dev);
};

struct pci_device_id {
    uint16_t vendor;
    uint16_t device;
};

int pci_register_driver(struct pci_driver *drv);
void pci_unregister_driver(struct pci_driver *drv);

#endif
