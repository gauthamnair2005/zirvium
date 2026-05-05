#include "drivers/zirv/driver.h"
#include "kernel/console.h"
#include <stdio.h>

static int intel_e1000_probe(void *hw_desc)
{
    (void)hw_desc;
    klog(LOG_OK, "NET ", "Intel E1000 Driver Loaded (Probing Success)");
    return 0;
}

const zirv_driver_t g_intel_e1000_driver = {
    .name = "Intel E1000",
    .component_tag = "NET ",
    .type = DRIVER_TYPE_PCI,
    .match.pci = {
        .vendor_id = 0x8086,
        .device_id = 0x100e
    },
    .probe = intel_e1000_probe
};
