/*
 * Intel IGBVF Ethernet variant 0
 * Category: ethernet
 * Vendor: Intel
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int net_intel_igbvf_0_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Intel IGBVF Ethernet variant 0: Device probed\n");
	return 0;
}

static int net_intel_igbvf_0_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Intel IGBVF Ethernet variant 0: Device removed\n");
	return 0;
}

static struct driver net_intel_igbvf_0_driver = {
	.name = "Intel IGBVF Ethernet variant 0",
	.probe = net_intel_igbvf_0_probe,
	.remove = net_intel_igbvf_0_remove,
};

static int __init net_intel_igbvf_0_init(void)
{
	printk(KERN_INFO "Intel IGBVF Ethernet variant 0: Initializing driver\n");
	return driver_register(&net_intel_igbvf_0_driver);
}

static void __exit net_intel_igbvf_0_exit(void)
{
	printk(KERN_INFO "Intel IGBVF Ethernet variant 0: Exiting driver\n");
	driver_unregister(&net_intel_igbvf_0_driver);
}

module_init(net_intel_igbvf_0_init);
module_exit(net_intel_igbvf_0_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Intel IGBVF Ethernet variant 0");
MODULE_VERSION("1.0");
