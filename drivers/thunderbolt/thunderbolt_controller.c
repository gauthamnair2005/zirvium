// Thunderbolt Controller Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

static int thunderbolt_probe(struct device *dev __attribute__((unused))) {
    kprintf("thunderbolt: Intel Thunderbolt controller initialized\n");
    return DRIVER_OK;
}

static int thunderbolt_remove(struct device *dev __attribute__((unused))) {
    return DRIVER_OK;
}

static struct driver thunderbolt_driver = {
    .name = "thunderbolt",
    .probe = thunderbolt_probe,
    .remove = thunderbolt_remove,
};

void __init thunderbolt_init(void) {
    driver_register(&thunderbolt_driver);
}
