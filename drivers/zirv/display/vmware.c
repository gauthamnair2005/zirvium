#include "drivers/zirv/driver.h"
#include "drivers/pci/pci.h"
#include "kernel/console.h"
#include <stdint.h>

#define VMWARE_SVGA_VENDOR_ID 0x15AD
#define VMWARE_SVGA_DEVICE_ID 0x0405

static int vmware_svga_probe(void *hw_desc) {
    pci_dev_t *pdev = (pci_dev_t *)hw_desc;
    pci_enable_device(pdev);

    void *fb_virt = pci_map_bar(pdev, 1); /* BAR1 is usually the framebuffer */
    if (!fb_virt) return -1;

    klog(LOG_OK, "VMW ", "VMware SVGA II detected, framebuffer at %p", pdev->bars[1].phys_addr);
    
    /* Basic initialization would go here */
    
    return 0;
}

const zirv_driver_t g_vmware_svga_driver = {
    .name = "vmware_svga",
    .component_tag = "VMW ",
    .type = DRIVER_TYPE_PCI,
    .match.pci = {
        .vendor_id = VMWARE_SVGA_VENDOR_ID,
        .device_id = VMWARE_SVGA_DEVICE_ID
    },
    .probe = vmware_svga_probe
};
