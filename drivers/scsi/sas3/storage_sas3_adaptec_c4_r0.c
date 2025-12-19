/*
 * Adaptec SAS3 4-channel Rev0
 * Category: storage_sas3
 * Vendor: Adaptec
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int storage_sas3_adaptec_c4_r0_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Adaptec SAS3 4-channel Rev0: Device probed\n");
	return 0;
}

static int storage_sas3_adaptec_c4_r0_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Adaptec SAS3 4-channel Rev0: Device removed\n");
	return 0;
}

static struct driver storage_sas3_adaptec_c4_r0_driver = {
	.name = "Adaptec SAS3 4-channel Rev0",
	.probe = storage_sas3_adaptec_c4_r0_probe,
	.remove = storage_sas3_adaptec_c4_r0_remove,
};

static int __init storage_sas3_adaptec_c4_r0_init(void)
{
	printk(KERN_INFO "Adaptec SAS3 4-channel Rev0: Initializing driver\n");
	return driver_register(&storage_sas3_adaptec_c4_r0_driver);
}

static void __exit storage_sas3_adaptec_c4_r0_exit(void)
{
	printk(KERN_INFO "Adaptec SAS3 4-channel Rev0: Exiting driver\n");
	driver_unregister(&storage_sas3_adaptec_c4_r0_driver);
}

module_init(storage_sas3_adaptec_c4_r0_init);
module_exit(storage_sas3_adaptec_c4_r0_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Adaptec SAS3 4-channel Rev0");
MODULE_VERSION("1.0");
