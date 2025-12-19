/*
 * Synaptics Touchpad Protocol 30
 * Category: input_touchpad
 * Vendor: Synaptics
 * Auto-generated driver stub for Zirvium
 */

#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/module.h>
#include <kernel/printk.h>

static int input_touchpad_synaptics_p30_probe(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Synaptics Touchpad Protocol 30: Device probed\n");
	return 0;
}

static int input_touchpad_synaptics_p30_remove(struct device *dev)
{
	(void)dev; /* Suppress unused warning */
	printk(KERN_INFO "Synaptics Touchpad Protocol 30: Device removed\n");
	return 0;
}

static struct driver input_touchpad_synaptics_p30_driver = {
	.name = "Synaptics Touchpad Protocol 30",
	.probe = input_touchpad_synaptics_p30_probe,
	.remove = input_touchpad_synaptics_p30_remove,
};

static int __init input_touchpad_synaptics_p30_init(void)
{
	printk(KERN_INFO "Synaptics Touchpad Protocol 30: Initializing driver\n");
	return driver_register(&input_touchpad_synaptics_p30_driver);
}

static void __exit input_touchpad_synaptics_p30_exit(void)
{
	printk(KERN_INFO "Synaptics Touchpad Protocol 30: Exiting driver\n");
	driver_unregister(&input_touchpad_synaptics_p30_driver);
}

module_init(input_touchpad_synaptics_p30_init);
module_exit(input_touchpad_synaptics_p30_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zirvium Project");
MODULE_DESCRIPTION("Synaptics Touchpad Protocol 30");
MODULE_VERSION("1.0");
