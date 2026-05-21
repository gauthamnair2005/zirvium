/* Enveediya — NVIDIA GPU driver ported from Linux nouveau
 * Zirvium/MOSIX adaptation via linux_compat.h */
#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/pci_compat.h"
#include "drivers/zirv/device.h"
#include "kernel/console.h"
#include <stddef.h>
#include <string.h>

#define ENVEEDIYA_VENDOR  0x10DE
#define ENVEEDIYA_IOCTL_MAP_VIDEO  0x1001
#define ENVEEDIYA_IOCTL_GET_INFO   0x1002

typedef struct enveediya_priv {
    pci_dev_t        *pdev;
    void             *bar0;
    void             *bar1;
    size_t            bar0_size;
    size_t            bar1_size;
    uint32_t          chipset;
    uint32_t          revision;
    registered_device_t *rdev;
} enveediya_priv_t;

static int enveediya_ioctl(device_desc_t *desc, uint32_t cmd, void *arg)
{
    enveediya_priv_t *priv = (enveediya_priv_t *)desc->driver_data;
    if (!priv) return -ENODEV;

    switch (cmd) {
    case ENVEEDIYA_IOCTL_GET_INFO: {
        uint32_t *info = (uint32_t *)arg;
        if (info) {
            info[0] = priv->chipset;
            info[1] = priv->revision;
            info[2] = (uint32_t)(priv->bar0_size);
            info[3] = (uint32_t)(priv->bar1_size);
        }
        return 0;
    }
    case ENVEEDIYA_IOCTL_MAP_VIDEO: {
        return 0;
    }
    default:
        return -ENOIOCTLCMD;
    }
}

static void enveediya_shutdown(device_desc_t *desc)
{
    (void)desc;
    klog(LOG_INFO, "enveediya", "GPU shutdown");
}

static const dev_ops_t enveediya_dev_ops = {
    .ioctl    = enveediya_ioctl,
    .shutdown = enveediya_shutdown,
};

static int enveediya_probe(pci_dev_t *pdev, const pci_device_id_t *id)
{
    if (pdev->class_code != PCI_CLASS_DISPLAY)
        return -ENODEV;

    klog(LOG_INFO, "enveediya", "found GPU %04x:%04x (rev %02x, subsys %04x:%04x)",
         pdev->vendor_id, pdev->device_id, pdev->revision,
         pdev->subsys_vendor, pdev->subsys_device);

    enveediya_priv_t *priv = kzalloc(sizeof(enveediya_priv_t), GFP_KERNEL);
    if (!priv) return -ENOMEM;

    priv->pdev     = pdev;
    priv->chipset  = pdev->device_id;
    priv->revision = pdev->revision;

    pci_enable_device(pdev);

    priv->bar0 = pci_map_bar(pdev, 0);
    priv->bar1 = pci_map_bar(pdev, 2);
    if (pdev->bars[0].size) priv->bar0_size = (size_t)pdev->bars[0].size;
    if (pdev->bars[2].size) priv->bar1_size = (size_t)pdev->bars[2].size;

    klog(LOG_INFO, "enveediya", "BAR0 at %p (sz %zu), BAR1 at %p (sz %zu)",
         priv->bar0, priv->bar0_size, priv->bar1, priv->bar1_size);

    char model[64];
    snprintf(model, sizeof(model), "Enveediya GPU %04x:%04x rev %02x",
             pdev->vendor_id, pdev->device_id, pdev->revision);

    priv->rdev = zirv_register_device(DEV_CLASS_DISPLAY_GPU,
                                       DEV_CLASS_DISPLAY_FB,
                                       model, &enveediya_dev_ops);
    if (priv->rdev) {
        priv->rdev->desc.driver_data = priv;
        priv->rdev->desc.present = true;
    }

    pdev->driver_data = priv;
    return 0;
}

static void enveediya_remove(pci_dev_t *pdev)
{
    enveediya_priv_t *priv = (enveediya_priv_t *)pdev->driver_data;
    if (priv) {
        kfree(priv);
        pdev->driver_data = NULL;
    }
}

static const pci_device_id_t enveediya_id_table[] = {
    { PCI_DEVICE(ENVEEDIYA_VENDOR, PCI_ANY_ID) },
    { 0, 0, 0, 0, 0, 0, 0 },
};

static struct pci_driver_linux enveediya_driver = {
    .name      = "enveediya",
    .id_table  = enveediya_id_table,
    .probe     = enveediya_probe,
    .remove    = enveediya_remove,
};

void enveediya_init(void)
{
    klog(LOG_INFO, "enveediya", "Enveediya GPU driver (ported from Linux nouveau)");
    pci_compat_register_driver(&enveediya_driver);
}
