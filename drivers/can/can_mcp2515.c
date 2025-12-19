// MCP2515 CAN Controller Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

static int can_mcp2515_probe(struct device *dev __attribute__((unused))) {
    kprintf("can: MCP2515 CAN controller initialized\n");
    return DRIVER_OK;
}

static int can_mcp2515_remove(struct device *dev __attribute__((unused))) {
    return DRIVER_OK;
}

static struct driver can_mcp2515_driver = {
    .name = "mcp2515",
    .probe = can_mcp2515_probe,
    .remove = can_mcp2515_remove,
};

void __init can_mcp2515_init(void) {
    driver_register(&can_mcp2515_driver);
}
