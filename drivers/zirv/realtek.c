#include "drivers/zirv/driver.h"
#include "kernel/console.h"
#include <stdio.h>

static int rtl8139_probe(void *hw_desc)
{
    (void)hw_desc;
    klog(LOG_OK, "NET ", "Realtek RTL8139 Fast Ethernet Controller Initialized");
    return 0;
}

const zirv_driver_t g_rtl8139_driver = {
    .name = "Realtek 8139",
    .component_tag = "NET ",
    .type = DRIVER_TYPE_PCI,
    .match.pci = {
        .vendor_id = 0x10ec,
        .device_id = 0x8139
    },
    .probe = rtl8139_probe
};
