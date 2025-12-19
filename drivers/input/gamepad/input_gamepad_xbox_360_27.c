/*
 * Xbox_360 Controller variant 27
 * Category: input_gamepad
 * Vendor: Xbox
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int input_gamepad_xbox_360_27_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Xbox_360 Controller variant 27: Device probed\n");
	return 0;
}

static int input_gamepad_xbox_360_27_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Xbox_360 Controller variant 27: Device removed\n");
	return 0;
}

static struct driver input_gamepad_xbox_360_27_driver = {
	.name = "Xbox_360 Controller variant 27",
	.probe = input_gamepad_xbox_360_27_probe,
	.remove = input_gamepad_xbox_360_27_remove,
};

static int __init input_gamepad_xbox_360_27_init(void)
{
	printk(KERN_INFO "Xbox_360 Controller variant 27: Initializing driver\n");
	return driver_register(&input_gamepad_xbox_360_27_driver);
}

static void __exit input_gamepad_xbox_360_27_exit(void)
{
	printk(KERN_INFO "Xbox_360 Controller variant 27: Exiting driver\n");
	driver_unregister(&input_gamepad_xbox_360_27_driver);
}

module_init(input_gamepad_xbox_360_27_init);
module_exit(input_gamepad_xbox_360_27_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Xbox_360 Controller variant 27");
MODULE_VERSION("1.0");
