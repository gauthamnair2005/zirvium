/*
 * OHCI NVIDIA Host Controller Rev4
 * Category: usb_host
 * Vendor: NVIDIA
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int usb_ohci_nvidia_4_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "OHCI NVIDIA Host Controller Rev4: Device probed\n");
	return 0;
}

static int usb_ohci_nvidia_4_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "OHCI NVIDIA Host Controller Rev4: Device removed\n");
	return 0;
}

static struct driver usb_ohci_nvidia_4_driver = {
	.name = "OHCI NVIDIA Host Controller Rev4",
	.probe = usb_ohci_nvidia_4_probe,
	.remove = usb_ohci_nvidia_4_remove,
};

static int __init usb_ohci_nvidia_4_init(void)
{
	printk(KERN_INFO "OHCI NVIDIA Host Controller Rev4: Initializing driver\n");
	return driver_register(&usb_ohci_nvidia_4_driver);
}

static void __exit usb_ohci_nvidia_4_exit(void)
{
	printk(KERN_INFO "OHCI NVIDIA Host Controller Rev4: Exiting driver\n");
	driver_unregister(&usb_ohci_nvidia_4_driver);
}

module_init(usb_ohci_nvidia_4_init);
module_exit(usb_ohci_nvidia_4_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("OHCI NVIDIA Host Controller Rev4");
MODULE_VERSION("1.0");
