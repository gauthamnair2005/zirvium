/*
 * Intel SATA3 2-port Rev4
 * Category: storage_sata
 * Vendor: Intel
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int storage_sata_intel_sata3_p2_r4_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Intel SATA3 2-port Rev4: Device probed\n");
	return 0;
}

static int storage_sata_intel_sata3_p2_r4_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Intel SATA3 2-port Rev4: Device removed\n");
	return 0;
}

static struct driver storage_sata_intel_sata3_p2_r4_driver = {
	.name = "Intel SATA3 2-port Rev4",
	.probe = storage_sata_intel_sata3_p2_r4_probe,
	.remove = storage_sata_intel_sata3_p2_r4_remove,
};

static int __init storage_sata_intel_sata3_p2_r4_init(void)
{
	printk(KERN_INFO "Intel SATA3 2-port Rev4: Initializing driver\n");
	return driver_register(&storage_sata_intel_sata3_p2_r4_driver);
}

static void __exit storage_sata_intel_sata3_p2_r4_exit(void)
{
	printk(KERN_INFO "Intel SATA3 2-port Rev4: Exiting driver\n");
	driver_unregister(&storage_sata_intel_sata3_p2_r4_driver);
}

module_init(storage_sata_intel_sata3_p2_r4_init);
module_exit(storage_sata_intel_sata3_p2_r4_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Intel SATA3 2-port Rev4");
MODULE_VERSION("1.0");
