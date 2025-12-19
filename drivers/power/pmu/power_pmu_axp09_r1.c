/*
 * PMU AXP09 Rev1
 * Category: power_pmu
 * Vendor: AXP
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int power_pmu_axp09_r1_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "PMU AXP09 Rev1: Device probed\n");
	return 0;
}

static int power_pmu_axp09_r1_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "PMU AXP09 Rev1: Device removed\n");
	return 0;
}

static struct driver power_pmu_axp09_r1_driver = {
	.name = "PMU AXP09 Rev1",
	.probe = power_pmu_axp09_r1_probe,
	.remove = power_pmu_axp09_r1_remove,
};

static int __init power_pmu_axp09_r1_init(void)
{
	printk(KERN_INFO "PMU AXP09 Rev1: Initializing driver\n");
	return driver_register(&power_pmu_axp09_r1_driver);
}

static void __exit power_pmu_axp09_r1_exit(void)
{
	printk(KERN_INFO "PMU AXP09 Rev1: Exiting driver\n");
	driver_unregister(&power_pmu_axp09_r1_driver);
}

module_init(power_pmu_axp09_r1_init);
module_exit(power_pmu_axp09_r1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("PMU AXP09 Rev1");
MODULE_VERSION("1.0");
