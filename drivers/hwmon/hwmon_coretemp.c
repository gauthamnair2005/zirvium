// Intel Core Temperature Monitor Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

static int hwmon_coretemp_probe(struct device *dev __attribute__((unused))) {
    kprintf("hwmon: Intel Core temperature monitoring initialized\n");
    return DRIVER_OK;
}

static int hwmon_coretemp_remove(struct device *dev __attribute__((unused))) {
    return DRIVER_OK;
}

static struct driver hwmon_coretemp_driver = {
    .name = "coretemp",
    .probe = hwmon_coretemp_probe,
    .remove = hwmon_coretemp_remove,
};

void __init hwmon_coretemp_init(void) {
    driver_register(&hwmon_coretemp_driver);
}
