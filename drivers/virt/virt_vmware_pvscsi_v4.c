/*
 * VMware PVSCSI version 4
 * Category: virtualization
 * Vendor: VMware
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int virt_vmware_pvscsi_v4_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "VMware PVSCSI version 4: Device probed\n");
	return 0;
}

static int virt_vmware_pvscsi_v4_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "VMware PVSCSI version 4: Device removed\n");
	return 0;
}

static struct driver virt_vmware_pvscsi_v4_driver = {
	.name = "VMware PVSCSI version 4",
	.probe = virt_vmware_pvscsi_v4_probe,
	.remove = virt_vmware_pvscsi_v4_remove,
};

static int __init virt_vmware_pvscsi_v4_init(void)
{
	printk(KERN_INFO "VMware PVSCSI version 4: Initializing driver\n");
	return driver_register(&virt_vmware_pvscsi_v4_driver);
}

static void __exit virt_vmware_pvscsi_v4_exit(void)
{
	printk(KERN_INFO "VMware PVSCSI version 4: Exiting driver\n");
	driver_unregister(&virt_vmware_pvscsi_v4_driver);
}

module_init(virt_vmware_pvscsi_v4_init);
module_exit(virt_vmware_pvscsi_v4_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("VMware PVSCSI version 4");
MODULE_VERSION("1.0");
