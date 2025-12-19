// Intel 6300ESB Watchdog Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

static int watchdog_i6300_probe(struct device *dev __attribute__((unused))) {
    kprintf("watchdog: Intel 6300ESB initialized\n");
    return DRIVER_OK;
}

static int watchdog_i6300_remove(struct device *dev __attribute__((unused))) {
    return DRIVER_OK;
}

static struct driver watchdog_i6300_driver = {
    .name = "i6300esb",
    .probe = watchdog_i6300_probe,
    .remove = watchdog_i6300_remove,
};

void __init watchdog_i6300_init(void) {
    driver_register(&watchdog_i6300_driver);
}
