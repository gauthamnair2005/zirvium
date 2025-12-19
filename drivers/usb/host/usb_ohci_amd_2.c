/*
 * OHCI AMD Host Controller Rev2
 * Category: usb_host
 * Vendor: AMD
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int usb_ohci_amd_2_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "OHCI AMD Host Controller Rev2: Device probed\n");
	return 0;
}

static int usb_ohci_amd_2_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "OHCI AMD Host Controller Rev2: Device removed\n");
	return 0;
}

static struct driver usb_ohci_amd_2_driver = {
	.name = "OHCI AMD Host Controller Rev2",
	.probe = usb_ohci_amd_2_probe,
	.remove = usb_ohci_amd_2_remove,
};

static int __init usb_ohci_amd_2_init(void)
{
	printk(KERN_INFO "OHCI AMD Host Controller Rev2: Initializing driver\n");
	return driver_register(&usb_ohci_amd_2_driver);
}

static void __exit usb_ohci_amd_2_exit(void)
{
	printk(KERN_INFO "OHCI AMD Host Controller Rev2: Exiting driver\n");
	driver_unregister(&usb_ohci_amd_2_driver);
}

module_init(usb_ohci_amd_2_init);
module_exit(usb_ohci_amd_2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("OHCI AMD Host Controller Rev2");
MODULE_VERSION("1.0");
