/*
 * UHCI Intel Host Controller Rev8
 * Category: usb_host
 * Vendor: Intel
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int usb_uhci_intel_8_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "UHCI Intel Host Controller Rev8: Device probed\n");
	return 0;
}

static int usb_uhci_intel_8_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "UHCI Intel Host Controller Rev8: Device removed\n");
	return 0;
}

static struct driver usb_uhci_intel_8_driver = {
	.name = "UHCI Intel Host Controller Rev8",
	.probe = usb_uhci_intel_8_probe,
	.remove = usb_uhci_intel_8_remove,
};

static int __init usb_uhci_intel_8_init(void)
{
	printk(KERN_INFO "UHCI Intel Host Controller Rev8: Initializing driver\n");
	return driver_register(&usb_uhci_intel_8_driver);
}

static void __exit usb_uhci_intel_8_exit(void)
{
	printk(KERN_INFO "UHCI Intel Host Controller Rev8: Exiting driver\n");
	driver_unregister(&usb_uhci_intel_8_driver);
}

module_init(usb_uhci_intel_8_init);
module_exit(usb_uhci_intel_8_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("UHCI Intel Host Controller Rev8");
MODULE_VERSION("1.0");
