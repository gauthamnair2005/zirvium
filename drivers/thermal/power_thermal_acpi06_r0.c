/*
 * Thermal ACPI06 Rev0
 * Category: power_thermal
 * Vendor: ACPI
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int power_thermal_acpi06_r0_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Thermal ACPI06 Rev0: Device probed\n");
	return 0;
}

static int power_thermal_acpi06_r0_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Thermal ACPI06 Rev0: Device removed\n");
	return 0;
}

static struct driver power_thermal_acpi06_r0_driver = {
	.name = "Thermal ACPI06 Rev0",
	.probe = power_thermal_acpi06_r0_probe,
	.remove = power_thermal_acpi06_r0_remove,
};

static int __init power_thermal_acpi06_r0_init(void)
{
	printk(KERN_INFO "Thermal ACPI06 Rev0: Initializing driver\n");
	return driver_register(&power_thermal_acpi06_r0_driver);
}

static void __exit power_thermal_acpi06_r0_exit(void)
{
	printk(KERN_INFO "Thermal ACPI06 Rev0: Exiting driver\n");
	driver_unregister(&power_thermal_acpi06_r0_driver);
}

module_init(power_thermal_acpi06_r0_init);
module_exit(power_thermal_acpi06_r0_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Thermal ACPI06 Rev0");
MODULE_VERSION("1.0");
