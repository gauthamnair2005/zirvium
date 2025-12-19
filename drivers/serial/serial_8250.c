// Serial 8250 UART Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

#define UART_BASE 0x3F8

static int serial_8250_probe(struct device *dev __attribute__((unused))) {
    kprintf("serial: 8250 UART initialized\n");
    return DRIVER_OK;
}

static int serial_8250_remove(struct device *dev __attribute__((unused))) {
    kprintf("serial: 8250 UART removed\n");
    return DRIVER_OK;
}

static struct driver serial_8250_driver = {
    .name = "serial_8250",
    .probe = serial_8250_probe,
    .remove = serial_8250_remove,
};

void __init serial_8250_init(void) {
    driver_register(&serial_8250_driver);
}
