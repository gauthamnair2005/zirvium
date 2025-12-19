/*
 * LSI SCSI 16-channel Rev1
 * Category: storage_scsi
 * Vendor: LSI
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int storage_scsi_lsi_c16_r1_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "LSI SCSI 16-channel Rev1: Device probed\n");
	return 0;
}

static int storage_scsi_lsi_c16_r1_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "LSI SCSI 16-channel Rev1: Device removed\n");
	return 0;
}

static struct driver storage_scsi_lsi_c16_r1_driver = {
	.name = "LSI SCSI 16-channel Rev1",
	.probe = storage_scsi_lsi_c16_r1_probe,
	.remove = storage_scsi_lsi_c16_r1_remove,
};

static int __init storage_scsi_lsi_c16_r1_init(void)
{
	printk(KERN_INFO "LSI SCSI 16-channel Rev1: Initializing driver\n");
	return driver_register(&storage_scsi_lsi_c16_r1_driver);
}

static void __exit storage_scsi_lsi_c16_r1_exit(void)
{
	printk(KERN_INFO "LSI SCSI 16-channel Rev1: Exiting driver\n");
	driver_unregister(&storage_scsi_lsi_c16_r1_driver);
}

module_init(storage_scsi_lsi_c16_r1_init);
module_exit(storage_scsi_lsi_c16_r1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("LSI SCSI 16-channel Rev1");
MODULE_VERSION("1.0");
