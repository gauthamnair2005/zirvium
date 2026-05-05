#include "drivers/zirv/driver.h"
#include "kernel/console.h"
#include <stdio.h>

static int virtio_net_probe(void *hw_desc)
{
    (void)hw_desc;
    klog(LOG_OK, "VIRT", "VirtIO Network Device Found and Initialized");
    return 0;
}

static int virtio_blk_probe(void *hw_desc)
{
    (void)hw_desc;
    klog(LOG_OK, "VIRT", "VirtIO Block Device Found and Initialized");
    return 0;
}

const zirv_driver_t g_virtio_net_driver = {
    .name = "VirtIO-Net",
    .component_tag = "VIRT",
    .type = DRIVER_TYPE_PCI,
    .match.pci = {
        .vendor_id = 0x1af4,
        .device_id = 0x1000
    },
    .probe = virtio_net_probe
};

const zirv_driver_t g_virtio_blk_driver = {
    .name = "VirtIO-Blk",
    .component_tag = "VIRT",
    .type = DRIVER_TYPE_PCI,
    .match.pci = {
        .vendor_id = 0x1af4,
        .device_id = 0x1001
    },
    .probe = virtio_blk_probe
};
