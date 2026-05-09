#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/pci_compat.h"
#include "drivers/zirv/device.h"
#include "kernel/console.h"
#include <stddef.h>
#include <string.h>

#define NVIDIA_VENDOR  0x10DE
#define NV_IOCTL_MAP_VIDEO  0x1001
#define NV_IOCTL_GET_INFO   0x1002

typedef struct nvidia_priv {
    pci_dev_t        *pdev;
    void             *bar0;
    void             *bar1;
    size_t            bar0_size;
    size_t            bar1_size;
    uint32_t          chipset;
    uint32_t          revision;
    registered_device_t *rdev;
} nvidia_priv_t;

static int nv_ioctl(device_desc_t *desc, uint32_t cmd, void *arg)
{
    nvidia_priv_t *priv = (nvidia_priv_t *)desc->driver_data;
    if (!priv) return -ENODEV;

    switch (cmd) {
    case NV_IOCTL_GET_INFO: {
        uint32_t *info = (uint32_t *)arg;
        if (info) {
            info[0] = priv->chipset;
            info[1] = priv->revision;
            info[2] = (uint32_t)(priv->bar0_size);
            info[3] = (uint32_t)(priv->bar1_size);
        }
        return 0;
    }
    case NV_IOCTL_MAP_VIDEO: {
        return 0;
    }
    default:
        return -ENOIOCTLCMD;
    }
}

static void nv_shutdown(device_desc_t *desc)
{
    (void)desc;
    klog(LOG_INFO, "nvidia", "GPU shutdown");
}

static const dev_ops_t nv_dev_ops = {
    .ioctl    = nv_ioctl,
    .shutdown = nv_shutdown,
};

static int nvidia_probe(pci_dev_t *pdev, const pci_device_id_t *id)
{
    if (pdev->class_code != PCI_CLASS_DISPLAY)
        return -ENODEV;

    klog(LOG_INFO, "nvidia", "found GPU %04x:%04x (rev %02x, subsys %04x:%04x)",
         pdev->vendor_id, pdev->device_id, pdev->revision,
         pdev->subsys_vendor, pdev->subsys_device);

    nvidia_priv_t *priv = kzalloc(sizeof(nvidia_priv_t), GFP_KERNEL);
    if (!priv) return -ENOMEM;

    priv->pdev     = pdev;
    priv->chipset  = pdev->device_id;
    priv->revision = pdev->revision;

    pci_enable_device(pdev);

    priv->bar0 = pci_map_bar(pdev, 0);
    priv->bar1 = pci_map_bar(pdev, 2);
    if (pdev->bars[0].size) priv->bar0_size = (size_t)pdev->bars[0].size;
    if (pdev->bars[2].size) priv->bar1_size = (size_t)pdev->bars[2].size;

    klog(LOG_INFO, "nvidia", "BAR0 at %p (sz %zu), BAR1 at %p (sz %zu)",
         priv->bar0, priv->bar0_size, priv->bar1, priv->bar1_size);

    char model[64];
    snprintf(model, sizeof(model), "NVIDIA GPU %04x:%04x rev %02x",
             pdev->vendor_id, pdev->device_id, pdev->revision);

    priv->rdev = zirv_register_device(DEV_CLASS_DISPLAY_GPU,
                                       DEV_CLASS_DISPLAY_FB,
                                       model, &nv_dev_ops);
    if (priv->rdev) {
        priv->rdev->desc.driver_data = priv;
        priv->rdev->desc.present = true;
    }

    pdev->driver_data = priv;
    return 0;
}

static void nvidia_remove(pci_dev_t *pdev)
{
    nvidia_priv_t *priv = (nvidia_priv_t *)pdev->driver_data;
    if (priv) {
        kfree(priv);
        pdev->driver_data = NULL;
    }
}

static const pci_device_id_t nvidia_id_table[] = {
    { PCI_DEVICE(NVIDIA_VENDOR, PCI_ANY_ID) },
    { 0, 0, 0, 0, 0, 0, 0 },
};

static struct pci_driver_linux nvidia_driver = {
    .name      = "nvidia",
    .id_table  = nvidia_id_table,
    .probe     = nvidia_probe,
    .remove    = nvidia_remove,
};

void nvidia_init(void)
{
    klog(LOG_INFO, "nvidia", "NVIDIA GPU driver (Linux compat)");
    pci_compat_register_driver(&nvidia_driver);
}
