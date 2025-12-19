/*
 * Charger BQ2415 Rev2
 * Category: power_charger
 * Vendor: BQ24
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int power_charger_bq2415_r2_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Charger BQ2415 Rev2: Device probed\n");
	return 0;
}

static int power_charger_bq2415_r2_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Charger BQ2415 Rev2: Device removed\n");
	return 0;
}

static struct driver power_charger_bq2415_r2_driver = {
	.name = "Charger BQ2415 Rev2",
	.probe = power_charger_bq2415_r2_probe,
	.remove = power_charger_bq2415_r2_remove,
};

static int __init power_charger_bq2415_r2_init(void)
{
	printk(KERN_INFO "Charger BQ2415 Rev2: Initializing driver\n");
	return driver_register(&power_charger_bq2415_r2_driver);
}

static void __exit power_charger_bq2415_r2_exit(void)
{
	printk(KERN_INFO "Charger BQ2415 Rev2: Exiting driver\n");
	driver_unregister(&power_charger_bq2415_r2_driver);
}

module_init(power_charger_bq2415_r2_init);
module_exit(power_charger_bq2415_r2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Charger BQ2415 Rev2");
MODULE_VERSION("1.0");
