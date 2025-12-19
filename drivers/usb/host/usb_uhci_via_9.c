/*
 * UHCI VIA Host Controller Rev9
 * Category: usb_host
 * Vendor: VIA
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int usb_uhci_via_9_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "UHCI VIA Host Controller Rev9: Device probed\n");
	return 0;
}

static int usb_uhci_via_9_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "UHCI VIA Host Controller Rev9: Device removed\n");
	return 0;
}

static struct driver usb_uhci_via_9_driver = {
	.name = "UHCI VIA Host Controller Rev9",
	.probe = usb_uhci_via_9_probe,
	.remove = usb_uhci_via_9_remove,
};

static int __init usb_uhci_via_9_init(void)
{
	printk(KERN_INFO "UHCI VIA Host Controller Rev9: Initializing driver\n");
	return driver_register(&usb_uhci_via_9_driver);
}

static void __exit usb_uhci_via_9_exit(void)
{
	printk(KERN_INFO "UHCI VIA Host Controller Rev9: Exiting driver\n");
	driver_unregister(&usb_uhci_via_9_driver);
}

module_init(usb_uhci_via_9_init);
module_exit(usb_uhci_via_9_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("UHCI VIA Host Controller Rev9");
MODULE_VERSION("1.0");
