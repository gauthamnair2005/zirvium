/*
 * Gyroscope MPU6050 SPI Rev0
 * Category: sensor_gyroscope
 * Vendor: MPU6050
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int sensor_gyroscope_mpu6050_spi_r0_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Gyroscope MPU6050 SPI Rev0: Device probed\n");
	return 0;
}

static int sensor_gyroscope_mpu6050_spi_r0_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Gyroscope MPU6050 SPI Rev0: Device removed\n");
	return 0;
}

static struct driver sensor_gyroscope_mpu6050_spi_r0_driver = {
	.name = "Gyroscope MPU6050 SPI Rev0",
	.probe = sensor_gyroscope_mpu6050_spi_r0_probe,
	.remove = sensor_gyroscope_mpu6050_spi_r0_remove,
};

static int __init sensor_gyroscope_mpu6050_spi_r0_init(void)
{
	printk(KERN_INFO "Gyroscope MPU6050 SPI Rev0: Initializing driver\n");
	return driver_register(&sensor_gyroscope_mpu6050_spi_r0_driver);
}

static void __exit sensor_gyroscope_mpu6050_spi_r0_exit(void)
{
	printk(KERN_INFO "Gyroscope MPU6050 SPI Rev0: Exiting driver\n");
	driver_unregister(&sensor_gyroscope_mpu6050_spi_r0_driver);
}

module_init(sensor_gyroscope_mpu6050_spi_r0_init);
module_exit(sensor_gyroscope_mpu6050_spi_r0_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Gyroscope MPU6050 SPI Rev0");
MODULE_VERSION("1.0");
