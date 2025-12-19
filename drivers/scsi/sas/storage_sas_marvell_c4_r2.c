/*
 * Marvell SAS 4-channel Rev2
 * Category: storage_sas
 * Vendor: Marvell
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int storage_sas_marvell_c4_r2_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Marvell SAS 4-channel Rev2: Device probed\n");
	return 0;
}

static int storage_sas_marvell_c4_r2_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Marvell SAS 4-channel Rev2: Device removed\n");
	return 0;
}

static struct driver storage_sas_marvell_c4_r2_driver = {
	.name = "Marvell SAS 4-channel Rev2",
	.probe = storage_sas_marvell_c4_r2_probe,
	.remove = storage_sas_marvell_c4_r2_remove,
};

static int __init storage_sas_marvell_c4_r2_init(void)
{
	printk(KERN_INFO "Marvell SAS 4-channel Rev2: Initializing driver\n");
	return driver_register(&storage_sas_marvell_c4_r2_driver);
}

static void __exit storage_sas_marvell_c4_r2_exit(void)
{
	printk(KERN_INFO "Marvell SAS 4-channel Rev2: Exiting driver\n");
	driver_unregister(&storage_sas_marvell_c4_r2_driver);
}

module_init(storage_sas_marvell_c4_r2_init);
module_exit(storage_sas_marvell_c4_r2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Marvell SAS 4-channel Rev2");
MODULE_VERSION("1.0");
