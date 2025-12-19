/*
 * VirtualBox Guest_Additions version 0
 * Category: virtualization
 * Vendor: VirtualBox
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int virt_virtualbox_guest_additions_v0_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "VirtualBox Guest_Additions version 0: Device probed\n");
	return 0;
}

static int virt_virtualbox_guest_additions_v0_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "VirtualBox Guest_Additions version 0: Device removed\n");
	return 0;
}

static struct driver virt_virtualbox_guest_additions_v0_driver = {
	.name = "VirtualBox Guest_Additions version 0",
	.probe = virt_virtualbox_guest_additions_v0_probe,
	.remove = virt_virtualbox_guest_additions_v0_remove,
};

static int __init virt_virtualbox_guest_additions_v0_init(void)
{
	printk(KERN_INFO "VirtualBox Guest_Additions version 0: Initializing driver\n");
	return driver_register(&virt_virtualbox_guest_additions_v0_driver);
}

static void __exit virt_virtualbox_guest_additions_v0_exit(void)
{
	printk(KERN_INFO "VirtualBox Guest_Additions version 0: Exiting driver\n");
	driver_unregister(&virt_virtualbox_guest_additions_v0_driver);
}

module_init(virt_virtualbox_guest_additions_v0_init);
module_exit(virt_virtualbox_guest_additions_v0_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("VirtualBox Guest_Additions version 0");
MODULE_VERSION("1.0");
