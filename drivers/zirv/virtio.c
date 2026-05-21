/* Zirvium VirtIO Drivers
 * Ported from Linux kernel drivers/virtio/virtio_ring.c,
 * drivers/block/virtio_blk.c, drivers/net/virtio_net.c
 *
 * Uses linux_compat.h + pci_compat_register_driver() for Linux-style probe */
#include "../../drivers/pci/pci_compat.h"

/* Declarations from the ported driver files */
extern struct pci_driver_linux virtio_blk_driver;
extern struct pci_driver_linux virtio_net_driver;

static int initialized = 0;

void virtio_init(void)
{
    if (initialized) return;
    initialized = 1;
    pci_compat_register_driver(&virtio_blk_driver);
    pci_compat_register_driver(&virtio_net_driver);
}
