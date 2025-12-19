// PCIe Root Complex Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

static int pcie_controller_probe(struct device *dev __attribute__((unused))) {
    kprintf("pcie: PCIe root complex initialized\n");
    return DRIVER_OK;
}

static int pcie_controller_remove(struct device *dev __attribute__((unused))) {
    return DRIVER_OK;
}

static struct driver pcie_controller_driver = {
    .name = "pcie_controller",
    .probe = pcie_controller_probe,
    .remove = pcie_controller_remove,
};

void __init pcie_controller_init(void) {
    driver_register(&pcie_controller_driver);
}
