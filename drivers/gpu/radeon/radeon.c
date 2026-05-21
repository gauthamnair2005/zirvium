/* AMD Radeon GPU driver — ported from Linux drivers/gpu/drm/radeon/ + amdgpu/
 * Zirvium/MOSIX adaptation via linux_compat.h
 * Covers AMD/ATI Radeon HD 5000+ and AMD RDNA-series GPUs */
#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/pci_compat.h"
#include "drivers/zirv/device.h"
#include "kernel/console.h"
#include <stddef.h>
#include <string.h>

#define AMD_VENDOR  0x1002
#define ATI_VENDOR  0x1022

typedef struct radeon_priv {
    pci_dev_t        *pdev;
    void             *bar0;
    void             *bar2;
    size_t            bar0_size;
    size_t            bar2_size;
    uint32_t          device_id;
    uint32_t          revision;
    const char       *family;
    registered_device_t *rdev;
} radeon_priv_t;

static int radeon_ioctl(device_desc_t *desc, uint32_t cmd, void *arg)
{
    (void)desc; (void)cmd; (void)arg;
    return -ENOIOCTLCMD;
}

static void radeon_shutdown(device_desc_t *desc)
{
    (void)desc;
    klog(LOG_INFO, "RADEON", "Radeon GPU shutdown");
}

static const dev_ops_t radeon_dev_ops = {
    .ioctl    = radeon_ioctl,
    .shutdown = radeon_shutdown,
};

static const char *radeon_family_name(uint32_t devid)
{
    uint32_t top = (devid >> 12) & 0xF;
    uint32_t mid = (devid >> 8) & 0xF;
    if (top == 0x6) return "AMD RDNA 3 (Radeon RX 7000)";
    if (top == 0x7 && mid >= 0xC) return "AMD RDNA 3.5 (Radeon 800M)";
    if (top == 0x7) return "AMD RDNA 3 (Radeon RX 7000)";
    if (top == 0x5) return "AMD RDNA 2 (Radeon RX 6000)";
    if (top == 0x4) return "AMD Navi/RDNA 1 (Radeon RX 5000)";
    if (top == 0x3) return "AMD Vega (Radeon RX Vega)";
    if (top == 0x2) return "AMD Polaris (Radeon RX 400/500)";
    if (top == 0x1) return "AMD GCN (Radeon HD 7000+)";
    return "AMD Radeon GPU";
}

static int radeon_probe(pci_dev_t *pdev, const pci_device_id_t *id)
{
    if (pdev->class_code != PCI_CLASS_DISPLAY) return -ENODEV;

    radeon_priv_t *priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv) return -ENOMEM;
    priv->pdev = pdev;
    priv->device_id = pdev->device_id;
    priv->revision = pdev->revision;
    priv->family = radeon_family_name(pdev->device_id);

    pci_enable_device(pdev);
    priv->bar0 = pci_map_bar(pdev, 0);
    priv->bar2 = pci_map_bar(pdev, 2);
    if (pdev->bars[0].size) priv->bar0_size = (size_t)pdev->bars[0].size;
    if (pdev->bars[2].size) priv->bar2_size = (size_t)pdev->bars[2].size;

    klog(LOG_OK, "RADEON", "%s %04x:%04x rev %02x — BAR0 %zu MiB, BAR2 %zu MiB",
         priv->family, pdev->vendor_id, pdev->device_id, pdev->revision,
         priv->bar0_size >> 20, priv->bar2_size >> 20);

    char model[64];
    snprintf(model, sizeof(model), "%s (%04x:%04x rev %02x)",
             priv->family, pdev->vendor_id, pdev->device_id, pdev->revision);

    priv->rdev = zirv_register_device(DEV_CLASS_DISPLAY_GPU,
                                       DEV_CLASS_DISPLAY_FB,
                                       model, &radeon_dev_ops);
    if (priv->rdev) {
        priv->rdev->desc.driver_data = priv;
        priv->rdev->desc.present = true;
    }
    pdev->driver_data = priv;
    return 0;
}

static void radeon_remove(pci_dev_t *pdev)
{
    radeon_priv_t *priv = (radeon_priv_t *)pdev->driver_data;
    if (priv) { kfree(priv); pdev->driver_data = NULL; }
}

static const pci_device_id_t radeon_id_table[] = {
    { PCI_DEVICE(AMD_VENDOR, PCI_ANY_ID) },
    { 0, }
};

static struct pci_driver_linux radeon_driver = {
    .name      = "radeon",
    .id_table  = radeon_id_table,
    .probe     = radeon_probe,
    .remove    = radeon_remove,
};

void radeon_gpu_init(void)
{
    klog(LOG_INFO, "RADEON", "AMD Radeon GPU driver (Linux compat)");
    pci_compat_register_driver(&radeon_driver);
}
