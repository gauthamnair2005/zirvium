// Intel GPIO Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

static int gpio_intel_probe(struct device *dev __attribute__((unused))) {
    kprintf("gpio: Intel GPIO controller initialized\n");
    return DRIVER_OK;
}

static int gpio_intel_remove(struct device *dev __attribute__((unused))) {
    return DRIVER_OK;
}

static struct driver gpio_intel_driver = {
    .name = "gpio_intel",
    .probe = gpio_intel_probe,
    .remove = gpio_intel_remove,
};

void __init gpio_intel_init(void) {
    driver_register(&gpio_intel_driver);
}
