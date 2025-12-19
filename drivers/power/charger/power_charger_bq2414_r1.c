/*
 * Charger BQ2414 Rev1
 * Category: power_charger
 * Vendor: BQ24
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int power_charger_bq2414_r1_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Charger BQ2414 Rev1: Device probed\n");
	return 0;
}

static int power_charger_bq2414_r1_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Charger BQ2414 Rev1: Device removed\n");
	return 0;
}

static struct driver power_charger_bq2414_r1_driver = {
	.name = "Charger BQ2414 Rev1",
	.probe = power_charger_bq2414_r1_probe,
	.remove = power_charger_bq2414_r1_remove,
};

static int __init power_charger_bq2414_r1_init(void)
{
	printk(KERN_INFO "Charger BQ2414 Rev1: Initializing driver\n");
	return driver_register(&power_charger_bq2414_r1_driver);
}

static void __exit power_charger_bq2414_r1_exit(void)
{
	printk(KERN_INFO "Charger BQ2414 Rev1: Exiting driver\n");
	driver_unregister(&power_charger_bq2414_r1_driver);
}

module_init(power_charger_bq2414_r1_init);
module_exit(power_charger_bq2414_r1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Charger BQ2414 Rev1");
MODULE_VERSION("1.0");
