/*
 * Fingerprint FPC1020 SPI Rev3
 * Category: sensor_fingerprint
 * Vendor: FPC1020
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int sensor_fingerprint_fpc1020_spi_r3_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Fingerprint FPC1020 SPI Rev3: Device probed\n");
	return 0;
}

static int sensor_fingerprint_fpc1020_spi_r3_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Fingerprint FPC1020 SPI Rev3: Device removed\n");
	return 0;
}

static struct driver sensor_fingerprint_fpc1020_spi_r3_driver = {
	.name = "Fingerprint FPC1020 SPI Rev3",
	.probe = sensor_fingerprint_fpc1020_spi_r3_probe,
	.remove = sensor_fingerprint_fpc1020_spi_r3_remove,
};

static int __init sensor_fingerprint_fpc1020_spi_r3_init(void)
{
	printk(KERN_INFO "Fingerprint FPC1020 SPI Rev3: Initializing driver\n");
	return driver_register(&sensor_fingerprint_fpc1020_spi_r3_driver);
}

static void __exit sensor_fingerprint_fpc1020_spi_r3_exit(void)
{
	printk(KERN_INFO "Fingerprint FPC1020 SPI Rev3: Exiting driver\n");
	driver_unregister(&sensor_fingerprint_fpc1020_spi_r3_driver);
}

module_init(sensor_fingerprint_fpc1020_spi_r3_init);
module_exit(sensor_fingerprint_fpc1020_spi_r3_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Fingerprint FPC1020 SPI Rev3");
MODULE_VERSION("1.0");
