// MDIO Bus Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

static int mdio_bus_probe(struct device *dev __attribute__((unused))) {
    kprintf("mdio: MDIO bus initialized\n");
    return DRIVER_OK;
}

static int mdio_bus_remove(struct device *dev __attribute__((unused))) {
    return DRIVER_OK;
}

static struct driver mdio_bus_driver = {
    .name = "mdio_bus",
    .probe = mdio_bus_probe,
    .remove = mdio_bus_remove,
};

void __init mdio_bus_init(void) {
    driver_register(&mdio_bus_driver);
}
