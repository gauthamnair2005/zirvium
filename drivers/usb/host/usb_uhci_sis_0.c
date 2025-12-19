/*
 * UHCI SIS Host Controller Rev0
 * Category: usb_host
 * Vendor: SIS
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int usb_uhci_sis_0_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "UHCI SIS Host Controller Rev0: Device probed\n");
	return 0;
}

static int usb_uhci_sis_0_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "UHCI SIS Host Controller Rev0: Device removed\n");
	return 0;
}

static struct driver usb_uhci_sis_0_driver = {
	.name = "UHCI SIS Host Controller Rev0",
	.probe = usb_uhci_sis_0_probe,
	.remove = usb_uhci_sis_0_remove,
};

static int __init usb_uhci_sis_0_init(void)
{
	printk(KERN_INFO "UHCI SIS Host Controller Rev0: Initializing driver\n");
	return driver_register(&usb_uhci_sis_0_driver);
}

static void __exit usb_uhci_sis_0_exit(void)
{
	printk(KERN_INFO "UHCI SIS Host Controller Rev0: Exiting driver\n");
	driver_unregister(&usb_uhci_sis_0_driver);
}

module_init(usb_uhci_sis_0_init);
module_exit(usb_uhci_sis_0_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("UHCI SIS Host Controller Rev0");
MODULE_VERSION("1.0");
