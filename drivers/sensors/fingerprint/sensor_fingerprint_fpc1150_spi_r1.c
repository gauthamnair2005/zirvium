/*
 * Fingerprint FPC1150 SPI Rev1
 * Category: sensor_fingerprint
 * Vendor: FPC1150
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int sensor_fingerprint_fpc1150_spi_r1_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Fingerprint FPC1150 SPI Rev1: Device probed\n");
	return 0;
}

static int sensor_fingerprint_fpc1150_spi_r1_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Fingerprint FPC1150 SPI Rev1: Device removed\n");
	return 0;
}

static struct driver sensor_fingerprint_fpc1150_spi_r1_driver = {
	.name = "Fingerprint FPC1150 SPI Rev1",
	.probe = sensor_fingerprint_fpc1150_spi_r1_probe,
	.remove = sensor_fingerprint_fpc1150_spi_r1_remove,
};

static int __init sensor_fingerprint_fpc1150_spi_r1_init(void)
{
	printk(KERN_INFO "Fingerprint FPC1150 SPI Rev1: Initializing driver\n");
	return driver_register(&sensor_fingerprint_fpc1150_spi_r1_driver);
}

static void __exit sensor_fingerprint_fpc1150_spi_r1_exit(void)
{
	printk(KERN_INFO "Fingerprint FPC1150 SPI Rev1: Exiting driver\n");
	driver_unregister(&sensor_fingerprint_fpc1150_spi_r1_driver);
}

module_init(sensor_fingerprint_fpc1150_spi_r1_init);
module_exit(sensor_fingerprint_fpc1150_spi_r1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Fingerprint FPC1150 SPI Rev1");
MODULE_VERSION("1.0");
