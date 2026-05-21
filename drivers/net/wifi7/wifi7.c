/* WiFi 7 (802.11be) Extreme High Throughput — PCI detection stub
 * Ported from Linux drivers/net/wireless/
 * Covers: Intel BE200/BE201, Qualcomm QCNCM865, MediaTek MT7927, Broadcom BCM4389
 * Zirvium/MOSIX adaptation via linux_compat.h */
#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/pci_compat.h"
#include "drivers/zirv/device.h"
#include "kernel/console.h"
#include <stddef.h>

#define PCI_CLASS_NET_WLAN      0x028000

typedef struct wifi7_priv {
    pci_dev_t        *pdev;
    void             *bar0;
    size_t            bar0_size;
    const char       *chip;
    registered_device_t *rdev;
} wifi7_priv_t;

static int wifi7_ioctl(device_desc_t *desc, uint32_t cmd, void *arg)
{
    (void)desc; (void)cmd; (void)arg;
    return -ENOIOCTLCMD;
}

static void wifi7_shutdown(device_desc_t *desc)
{
    (void)desc;
}

static const dev_ops_t wifi7_dev_ops = {
    .ioctl    = wifi7_ioctl,
    .shutdown = wifi7_shutdown,
};

static const char *wifi7_chip_name(uint16_t vendor, uint16_t device)
{
    if (vendor == 0x8086) {
        switch (device) {
        case 0x2727: return "Intel BE200";
        case 0x2728: return "Intel BE201";
        case 0x2729: return "Intel BE202";
        case 0x2730: return "Intel BE200 (non-vPro)";
        case 0x2731: return "Intel BE201 (non-vPro)";
        }
    }
    if (vendor == 0x14C3) { /* MediaTek */
        switch (device) {
        case 0x0616: return "MediaTek MT7927";
        case 0x0617: return "MediaTek MT7927A";
        case 0x7927: return "MediaTek MT7927";
        }
    }
    if (vendor == 0x17CB) { /* Qualcomm */
        switch (device) {
        case 0x1103: return "Qualcomm QCNCM865";
        }
    }
    if (vendor == 0x14E4) { /* Broadcom */
        switch (device) {
        case 0x4433: return "Broadcom BCM4389";
        case 0x4466: return "Broadcom BCM4390";
        }
    }
    return "WiFi 7 (802.11be) adapter";
}

static int wifi7_probe(pci_dev_t *pdev, const pci_device_id_t *id)
{
    (void)id;
    if (pdev->class_code != 0x02 || pdev->subclass != 0x80)
        return -ENODEV;

    wifi7_priv_t *priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv) return -ENOMEM;
    priv->pdev = pdev;
    priv->chip = wifi7_chip_name(pdev->vendor_id, pdev->device_id);

    pci_enable_device(pdev);
    priv->bar0 = pci_map_bar(pdev, 0);
    if (pdev->bars[0].size) priv->bar0_size = (size_t)pdev->bars[0].size;

    klog(LOG_OK, "WIFI7", "%s detected — %04x:%04x rev %02x, BAR0 %zu KiB",
         priv->chip, pdev->vendor_id, pdev->device_id, pdev->revision,
         priv->bar0_size >> 10);

    char model[64];
    snprintf(model, sizeof(model), "%s (WiFi 7 / 802.11be EHT)", priv->chip);

    priv->rdev = zirv_register_device(DEV_CLASS_NET_WLAN,
                                       DEV_CLASS_NET_WLAN,
                                       model, &wifi7_dev_ops);
    if (priv->rdev) {
        priv->rdev->desc.driver_data = priv;
        priv->rdev->desc.present = true;
    }
    pdev->driver_data = priv;
    return 0;
}

static void wifi7_remove(pci_dev_t *pdev)
{
    wifi7_priv_t *priv = (wifi7_priv_t *)pdev->driver_data;
    if (priv) { kfree(priv); pdev->driver_data = NULL; }
}

static const pci_device_id_t wifi7_id_table[] = {
    /* Intel BE200/BE201 — WiFi 7 CNVio2 */
    { PCI_DEVICE(0x8086, 0x2727) },
    { PCI_DEVICE(0x8086, 0x2728) },
    { PCI_DEVICE(0x8086, 0x2729) },
    { PCI_DEVICE(0x8086, 0x2730) },
    { PCI_DEVICE(0x8086, 0x2731) },
    /* MediaTek MT7927 */
    { PCI_DEVICE(0x14C3, 0x0616) },
    { PCI_DEVICE(0x14C3, 0x0617) },
    { PCI_DEVICE(0x14C3, 0x7927) },
    /* Qualcomm QCNCM865 */
    { PCI_DEVICE(0x17CB, 0x1103) },
    /* Broadcom BCM4389 */
    { PCI_DEVICE(0x14E4, 0x4433) },
    { PCI_DEVICE(0x14E4, 0x4466) },
    { 0, }
};

static struct pci_driver_linux wifi7_driver = {
    .name      = "wifi7",
    .id_table  = wifi7_id_table,
    .probe     = wifi7_probe,
    .remove    = wifi7_remove,
};

void wifi7_init(void)
{
    klog(LOG_INFO, "WIFI7", "WiFi 7 (802.11be) adapter driver (Linux compat)");
    pci_compat_register_driver(&wifi7_driver);
}
