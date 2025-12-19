/*
 * Gyroscope L3GD20 SPI Rev6
 * Category: sensor_gyroscope
 * Vendor: L3GD20
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int sensor_gyroscope_l3gd20_spi_r6_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Gyroscope L3GD20 SPI Rev6: Device probed\n");
	return 0;
}

static int sensor_gyroscope_l3gd20_spi_r6_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Gyroscope L3GD20 SPI Rev6: Device removed\n");
	return 0;
}

static struct driver sensor_gyroscope_l3gd20_spi_r6_driver = {
	.name = "Gyroscope L3GD20 SPI Rev6",
	.probe = sensor_gyroscope_l3gd20_spi_r6_probe,
	.remove = sensor_gyroscope_l3gd20_spi_r6_remove,
};

static int __init sensor_gyroscope_l3gd20_spi_r6_init(void)
{
	printk(KERN_INFO "Gyroscope L3GD20 SPI Rev6: Initializing driver\n");
	return driver_register(&sensor_gyroscope_l3gd20_spi_r6_driver);
}

static void __exit sensor_gyroscope_l3gd20_spi_r6_exit(void)
{
	printk(KERN_INFO "Gyroscope L3GD20 SPI Rev6: Exiting driver\n");
	driver_unregister(&sensor_gyroscope_l3gd20_spi_r6_driver);
}

module_init(sensor_gyroscope_l3gd20_spi_r6_init);
module_exit(sensor_gyroscope_l3gd20_spi_r6_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Gyroscope L3GD20 SPI Rev6");
MODULE_VERSION("1.0");
