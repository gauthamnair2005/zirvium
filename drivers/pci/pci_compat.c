#include "pci.h"
#include "drivers/compat/linux_compat.h"
#include "kernel/console.h"
#include <stddef.h>

int pci_compat_register_driver(struct pci_driver_linux *ldrv)
{
    if (!ldrv || !ldrv->id_table || !ldrv->probe)
        return -EINVAL;

    for (size_t di = 0; di < pci_device_count(); di++) {
        pci_dev_t *dev = pci_get_device(di);
        if (!dev) continue;

        for (const pci_device_id_t *id = ldrv->id_table;
             id->vendor || id->device; id++) {

            int match = 1;
            if (id->vendor != PCI_ANY_ID && id->vendor != dev->vendor_id)
                match = 0;
            if (id->device != PCI_ANY_ID && id->device != dev->device_id)
                match = 0;
            if (!match) continue;

            klog(LOG_INFO, ldrv->name, "matched %04x:%04x (rev %02x)",
                 dev->vendor_id, dev->device_id, dev->revision);

            int ret = ldrv->probe(dev, id);
            if (ret == 0) {
                dev->driver_name = ldrv->name;
                klog(LOG_OK, ldrv->name, "probe OK for %04x:%04x",
                     dev->vendor_id, dev->device_id);
                break;
            }
            klog(LOG_WARN, ldrv->name, "probe failed (%d) for %04x:%04x",
                 ret, dev->vendor_id, dev->device_id);
        }
    }
    return 0;
}
