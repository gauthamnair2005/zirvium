#ifndef ZIRVIUM_PCI_COMPAT_H
#define ZIRVIUM_PCI_COMPAT_H

#include "drivers/compat/linux_compat.h"

int pci_compat_register_driver(struct pci_driver_linux *drv);

#endif
