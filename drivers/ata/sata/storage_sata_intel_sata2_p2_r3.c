/*
 * Intel SATA2 2-port Rev3
 * Category: storage_sata
 * Vendor: Intel
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int storage_sata_intel_sata2_p2_r3_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Intel SATA2 2-port Rev3: Device probed\n");
	return 0;
}

static int storage_sata_intel_sata2_p2_r3_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Intel SATA2 2-port Rev3: Device removed\n");
	return 0;
}

static struct driver storage_sata_intel_sata2_p2_r3_driver = {
	.name = "Intel SATA2 2-port Rev3",
	.probe = storage_sata_intel_sata2_p2_r3_probe,
	.remove = storage_sata_intel_sata2_p2_r3_remove,
};

static int __init storage_sata_intel_sata2_p2_r3_init(void)
{
	printk(KERN_INFO "Intel SATA2 2-port Rev3: Initializing driver\n");
	return driver_register(&storage_sata_intel_sata2_p2_r3_driver);
}

static void __exit storage_sata_intel_sata2_p2_r3_exit(void)
{
	printk(KERN_INFO "Intel SATA2 2-port Rev3: Exiting driver\n");
	driver_unregister(&storage_sata_intel_sata2_p2_r3_driver);
}

module_init(storage_sata_intel_sata2_p2_r3_init);
module_exit(storage_sata_intel_sata2_p2_r3_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Intel SATA2 2-port Rev3");
MODULE_VERSION("1.0");
