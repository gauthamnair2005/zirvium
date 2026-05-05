#include "driver.h"
#include "drivers/pci/pci.h"
#include "kernel/console.h"
#include <stddef.h>

#define MAX_DRIVERS 64

static const zirv_driver_t *g_drivers[MAX_DRIVERS];
static size_t g_driver_count = 0;

void driver_register(const zirv_driver_t *drv) {
    if (g_driver_count < MAX_DRIVERS) {
        g_drivers[g_driver_count++] = drv;
    }
}

static void probe_pci_devices(void) {
    size_t dev_count = pci_device_count();
    for (size_t i = 0; i < dev_count; i++) {
        pci_dev_t *dev = pci_get_device(i);
        bool matched = false;

        for (size_t j = 0; j < g_driver_count; j++) {
            const zirv_driver_t *drv = g_drivers[j];
            if (drv->type != DRIVER_TYPE_PCI) continue;

            if (drv->match.pci.vendor_id == dev->vendor_id &&
                drv->match.pci.device_id == dev->device_id) {
                
                int res = drv->probe(dev);
                if (res == 0) {
                    klog(LOG_OK, drv->component_tag, "Driver loaded for %04x:%04x", 
                         dev->vendor_id, dev->device_id);
                    matched = true;
                    break;
                }
            }
        }
    }
}

void driver_probe_all(void) {
    /* Step 1: PCI discovery */
    probe_pci_devices();

    /* Step 2: Platform discovery (future ACPI integration) */
}
