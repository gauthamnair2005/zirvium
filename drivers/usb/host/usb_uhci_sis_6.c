/*
 * UHCI SIS Host Controller Rev6
 * Category: usb_host
 * Vendor: SIS
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int usb_uhci_sis_6_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "UHCI SIS Host Controller Rev6: Device probed\n");
	return 0;
}

static int usb_uhci_sis_6_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "UHCI SIS Host Controller Rev6: Device removed\n");
	return 0;
}

static struct driver usb_uhci_sis_6_driver = {
	.name = "UHCI SIS Host Controller Rev6",
	.probe = usb_uhci_sis_6_probe,
	.remove = usb_uhci_sis_6_remove,
};

static int __init usb_uhci_sis_6_init(void)
{
	printk(KERN_INFO "UHCI SIS Host Controller Rev6: Initializing driver\n");
	return driver_register(&usb_uhci_sis_6_driver);
}

static void __exit usb_uhci_sis_6_exit(void)
{
	printk(KERN_INFO "UHCI SIS Host Controller Rev6: Exiting driver\n");
	driver_unregister(&usb_uhci_sis_6_driver);
}

module_init(usb_uhci_sis_6_init);
module_exit(usb_uhci_sis_6_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("UHCI SIS Host Controller Rev6");
MODULE_VERSION("1.0");
