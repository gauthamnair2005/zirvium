/*
 * Intel SATA2 4-port Rev6
 * Category: storage_sata
 * Vendor: Intel
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int storage_sata_intel_sata2_p4_r6_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Intel SATA2 4-port Rev6: Device probed\n");
	return 0;
}

static int storage_sata_intel_sata2_p4_r6_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Intel SATA2 4-port Rev6: Device removed\n");
	return 0;
}

static struct driver storage_sata_intel_sata2_p4_r6_driver = {
	.name = "Intel SATA2 4-port Rev6",
	.probe = storage_sata_intel_sata2_p4_r6_probe,
	.remove = storage_sata_intel_sata2_p4_r6_remove,
};

static int __init storage_sata_intel_sata2_p4_r6_init(void)
{
	printk(KERN_INFO "Intel SATA2 4-port Rev6: Initializing driver\n");
	return driver_register(&storage_sata_intel_sata2_p4_r6_driver);
}

static void __exit storage_sata_intel_sata2_p4_r6_exit(void)
{
	printk(KERN_INFO "Intel SATA2 4-port Rev6: Exiting driver\n");
	driver_unregister(&storage_sata_intel_sata2_p4_r6_driver);
}

module_init(storage_sata_intel_sata2_p4_r6_init);
module_exit(storage_sata_intel_sata2_p4_r6_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Intel SATA2 4-port Rev6");
MODULE_VERSION("1.0");
