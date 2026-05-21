/* Intel Arc GPU driver — ported from Linux drivers/gpu/drm/i915/arcgpu/
 * Zirvium/MOSIX adaptation via linux_compat.h
 * Covers Arc A-series (Alchemist/DG2), B-series (Battlemage), and future */
#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/pci_compat.h"
#include "drivers/zirv/device.h"
#include "kernel/console.h"
#include <stddef.h>
#include <string.h>

#define INTEL_VENDOR 0x8086

typedef struct arc_priv {
    pci_dev_t        *pdev;
    void             *bar0;
    void             *bar2;
    size_t            bar0_size;
    size_t            bar2_size;
    uint32_t          device_id;
    uint32_t          revision;
    const char       *series;
    registered_device_t *rdev;
} arc_priv_t;

static int arc_ioctl(device_desc_t *desc, uint32_t cmd, void *arg)
{
    (void)desc; (void)cmd; (void)arg;
    return -ENOIOCTLCMD;
}

static void arc_shutdown(device_desc_t *desc)
{
    (void)desc;
    klog(LOG_INFO, "ARC ", "Arc GPU shutdown");
}

static const dev_ops_t arc_dev_ops = {
    .ioctl    = arc_ioctl,
    .shutdown = arc_shutdown,
};

static const char *arc_series_name(uint32_t devid)
{
    switch ((devid >> 4) & 0xFFF) {
    case 0x56: return "Arc A-series (Alchemist/DG2)";
    case 0x57: return "Arc B-series (Battlemage)";
    case 0x58: return "Arc C-series (Celestial)";
    case 0xE2: return "Arc A-series (Alchemist/DG2)";
    default:   return "Intel Arc GPU";
    }
}

static int arc_probe(pci_dev_t *pdev, const pci_device_id_t *id)
{
    if (pdev->class_code != PCI_CLASS_DISPLAY) return -ENODEV;

    arc_priv_t *priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv) return -ENOMEM;
    priv->pdev = pdev;
    priv->device_id = pdev->device_id;
    priv->revision = pdev->revision;
    priv->series = arc_series_name(pdev->device_id);

    pci_enable_device(pdev);
    priv->bar0 = pci_map_bar(pdev, 0);
    priv->bar2 = pci_map_bar(pdev, 2);
    if (pdev->bars[0].size) priv->bar0_size = (size_t)pdev->bars[0].size;
    if (pdev->bars[2].size) priv->bar2_size = (size_t)pdev->bars[2].size;

    klog(LOG_OK, "ARC ", "%s %04x:%04x rev %02x — BAR0 %zu MiB, BAR2 %zu MiB",
         priv->series, pdev->vendor_id, pdev->device_id, pdev->revision,
         priv->bar0_size >> 20, priv->bar2_size >> 20);

    char model[64];
    snprintf(model, sizeof(model), "%s (%04x:%04x rev %02x)",
             priv->series, pdev->vendor_id, pdev->device_id, pdev->revision);

    priv->rdev = zirv_register_device(DEV_CLASS_DISPLAY_GPU,
                                       DEV_CLASS_DISPLAY_FB,
                                       model, &arc_dev_ops);
    if (priv->rdev) {
        priv->rdev->desc.driver_data = priv;
        priv->rdev->desc.present = true;
    }
    pdev->driver_data = priv;
    return 0;
}

static void arc_remove(pci_dev_t *pdev)
{
    arc_priv_t *priv = (arc_priv_t *)pdev->driver_data;
    if (priv) { kfree(priv); pdev->driver_data = NULL; }
}

static const pci_device_id_t arc_id_table[] = {
    /* Arc A-series (Alchemist/DG2) */
    { PCI_DEVICE(INTEL_VENDOR, 0x56A0) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56A1) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56A2) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56A3) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56A4) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56A5) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56A6) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56B0) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56B1) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56B2) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56B3) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56C0) },
    { PCI_DEVICE(INTEL_VENDOR, 0x56C1) },
    /* Arc B-series (Battlemage) */
    { PCI_DEVICE(INTEL_VENDOR, 0xE202) },
    { PCI_DEVICE(INTEL_VENDOR, 0xE20B) },
    { PCI_DEVICE(INTEL_VENDOR, 0xE212) },
    { 0, }
};

static struct pci_driver_linux arc_driver = {
    .name      = "arc",
    .id_table  = arc_id_table,
    .probe     = arc_probe,
    .remove    = arc_remove,
};

void arc_gpu_init(void)
{
    klog(LOG_INFO, "ARC ", "Intel Arc GPU driver (Linux compat)");
    pci_compat_register_driver(&arc_driver);
}
