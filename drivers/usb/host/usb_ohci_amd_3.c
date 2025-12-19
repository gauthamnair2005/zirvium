/*
 * OHCI AMD Host Controller Rev3
 * Category: usb_host
 * Vendor: AMD
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int usb_ohci_amd_3_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "OHCI AMD Host Controller Rev3: Device probed\n");
	return 0;
}

static int usb_ohci_amd_3_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "OHCI AMD Host Controller Rev3: Device removed\n");
	return 0;
}

static struct driver usb_ohci_amd_3_driver = {
	.name = "OHCI AMD Host Controller Rev3",
	.probe = usb_ohci_amd_3_probe,
	.remove = usb_ohci_amd_3_remove,
};

static int __init usb_ohci_amd_3_init(void)
{
	printk(KERN_INFO "OHCI AMD Host Controller Rev3: Initializing driver\n");
	return driver_register(&usb_ohci_amd_3_driver);
}

static void __exit usb_ohci_amd_3_exit(void)
{
	printk(KERN_INFO "OHCI AMD Host Controller Rev3: Exiting driver\n");
	driver_unregister(&usb_ohci_amd_3_driver);
}

module_init(usb_ohci_amd_3_init);
module_exit(usb_ohci_amd_3_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("OHCI AMD Host Controller Rev3");
MODULE_VERSION("1.0");
