/* Thunderbolt / USB4 PCI detection stub
 * Ported from Linux drivers/thunderbolt/
 * Zirvium/MOSIX adaptation via linux_compat.h */
#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/pci_compat.h"
#include "drivers/zirv/device.h"
#include "kernel/console.h"
#include <stddef.h>

/* Thunderbolt controllers appear as PCI bridges (class 0x06) with specific
 * vendor/device IDs, or as NHI (Native Host Interface) devices at class 0x0C
 * (serial bus controller, subclass 0x07, prog-if 0x00).  USB4 is class 0x0C,
 * subclass 0x03, prog-if 0x40 (USB4 host controller). */
#define PCI_CLASS_SERIAL_USB4    0x0C0340

typedef struct tb_priv {
    pci_dev_t        *pdev;
    void             *bar0;
    size_t            bar0_size;
    int               is_usb4;
    registered_device_t *rdev;
} tb_priv_t;

static int tb_ioctl(device_desc_t *desc, uint32_t cmd, void *arg)
{
    (void)desc; (void)cmd; (void)arg;
    return -ENOIOCTLCMD;
}

static void tb_shutdown(device_desc_t *desc)
{
    (void)desc;
    klog(LOG_INFO, "TB  ", "Thunderbolt controller shutdown");
}

static const dev_ops_t tb_dev_ops = {
    .ioctl    = tb_ioctl,
    .shutdown = tb_shutdown,
};

static int tb_probe(pci_dev_t *pdev, const pci_device_id_t *id)
{
    (void)id;
    /* Thunderbolt NHI class check */
    if (pdev->class_code != 0x0C && pdev->class_code != 0x06)
        return -ENODEV;

    tb_priv_t *priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv) return -ENOMEM;
    priv->pdev = pdev;
    priv->is_usb4 = (pdev->class_code == 0x0C && pdev->prog_if == 0x40);

    pci_enable_device(pdev);
    priv->bar0 = pci_map_bar(pdev, 0);
    if (pdev->bars[0].size) priv->bar0_size = (size_t)pdev->bars[0].size;

    klog(LOG_OK, "TB  ", "%s controller %04x:%04x rev %02x — BAR0 %zu KiB",
         priv->is_usb4 ? "USB4" : "Thunderbolt",
         pdev->vendor_id, pdev->device_id, pdev->revision,
         priv->bar0_size >> 10);

    char model[64];
    snprintf(model, sizeof(model), "%s Controller %04x:%04x rev %02x",
             priv->is_usb4 ? "USB4" : "Thunderbolt",
             pdev->vendor_id, pdev->device_id, pdev->revision);

    priv->rdev = zirv_register_device(
        priv->is_usb4 ? DEV_CLASS_USB : DEV_CLASS_NET_ETH,
        DEV_CLASS_UNKNOWN, model, &tb_dev_ops);
    if (priv->rdev) {
        priv->rdev->desc.driver_data = priv;
        priv->rdev->desc.present = true;
    }
    pdev->driver_data = priv;
    return 0;
}

static void tb_remove(pci_dev_t *pdev)
{
    tb_priv_t *priv = (tb_priv_t *)pdev->driver_data;
    if (priv) { kfree(priv); pdev->driver_data = NULL; }
}

/* Thunderbolt 3/4 controllers and USB4 host interfaces */
static const pci_device_id_t tb_id_table[] = {
    { PCI_DEVICE(0x8086, 0x15D0) },  /* Intel Ice Lake TBT3 */
    { PCI_DEVICE(0x8086, 0x15D1) },  /* Intel Ice Lake TBT3 */
    { PCI_DEVICE(0x8086, 0x15D2) },  /* Intel Ice Lake TBT3 */
    { PCI_DEVICE(0x8086, 0x15D3) },  /* Intel Ice Lake TBT3 */
    { PCI_DEVICE(0x8086, 0x15DA) },  /* Intel Tiger Lake TBT3 */
    { PCI_DEVICE(0x8086, 0x15DB) },  /* Intel Tiger Lake TBT3 */
    { PCI_DEVICE(0x8086, 0x15DC) },  /* Intel Tiger Lake TBT3 */
    { PCI_DEVICE(0x8086, 0x15DD) },  /* Intel Tiger Lake TBT3 */
    { PCI_DEVICE(0x8086, 0x15DE) },  /* Intel Tiger Lake TBT3 */
    { PCI_DEVICE(0x8086, 0x15DF) },  /* Intel Tiger Lake TBT3 */
    { PCI_DEVICE(0x8086, 0x15E0) },  /* Intel Alder Lake TBT4 */
    { PCI_DEVICE(0x8086, 0x15E1) },  /* Intel Alder Lake TBT4 */
    { PCI_DEVICE(0x8086, 0x15E2) },  /* Intel Alder Lake TBT4 */
    { PCI_DEVICE(0x8086, 0x15E3) },  /* Intel Alder Lake TBT4 */
    { PCI_DEVICE(0x8086, 0x15E4) },  /* Intel Meteor Lake TBT4/USB4 */
    { PCI_DEVICE(0x8086, 0x15E5) },  /* Intel Meteor Lake TBT4/USB4 */
    { PCI_DEVICE(0x8086, 0x15E6) },  /* Intel Meteor Lake TBT4/USB4 */
    { PCI_DEVICE(0x8086, 0x15E7) },  /* Intel Meteor Lake TBT4/USB4 */
    { PCI_DEVICE(0x8086, 0x15F0) },  /* Intel Barlow Ridge USB4 */
    { PCI_DEVICE(0x8086, 0x15F1) },  /* Intel Barlow Ridge USB4 */
    { PCI_DEVICE(0x8086, 0x15F2) },  /* Intel Barlow Ridge USB4 */
    { PCI_DEVICE(0x8086, 0x15F3) },  /* Intel Barlow Ridge USB4 */
    { PCI_DEVICE(0x8086, 0x1136) },  /* Intel USB4 host */
    { PCI_DEVICE(0x8086, 0x1137) },  /* Intel USB4 host */
    { PCI_DEVICE(0x1022, 0x43D0) },  /* AMD USB4 */
    { PCI_DEVICE(0x1022, 0x43D1) },  /* AMD USB4 */
    { 0, }
};

static struct pci_driver_linux tb_driver = {
    .name      = "thunderbolt",
    .id_table  = tb_id_table,
    .probe     = tb_probe,
    .remove    = tb_remove,
};

void thunderbolt_init(void)
{
    klog(LOG_INFO, "TB  ", "Thunderbolt/USB4 driver (Linux compat)");
    pci_compat_register_driver(&tb_driver);
}
