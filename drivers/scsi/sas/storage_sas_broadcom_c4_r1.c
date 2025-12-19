/*
 * Broadcom SAS 4-channel Rev1
 * Category: storage_sas
 * Vendor: Broadcom
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int storage_sas_broadcom_c4_r1_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Broadcom SAS 4-channel Rev1: Device probed\n");
	return 0;
}

static int storage_sas_broadcom_c4_r1_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Broadcom SAS 4-channel Rev1: Device removed\n");
	return 0;
}

static struct driver storage_sas_broadcom_c4_r1_driver = {
	.name = "Broadcom SAS 4-channel Rev1",
	.probe = storage_sas_broadcom_c4_r1_probe,
	.remove = storage_sas_broadcom_c4_r1_remove,
};

static int __init storage_sas_broadcom_c4_r1_init(void)
{
	printk(KERN_INFO "Broadcom SAS 4-channel Rev1: Initializing driver\n");
	return driver_register(&storage_sas_broadcom_c4_r1_driver);
}

static void __exit storage_sas_broadcom_c4_r1_exit(void)
{
	printk(KERN_INFO "Broadcom SAS 4-channel Rev1: Exiting driver\n");
	driver_unregister(&storage_sas_broadcom_c4_r1_driver);
}

module_init(storage_sas_broadcom_c4_r1_init);
module_exit(storage_sas_broadcom_c4_r1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Broadcom SAS 4-channel Rev1");
MODULE_VERSION("1.0");
