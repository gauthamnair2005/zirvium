// Serial 8250 UART Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>
#include <arch/x64.h>

#define UART_BASE 0x3F8

static int serial_initialized = 0;

static void serial_configure_baud_rate(uint16_t divisor) {
    outb(UART_BASE + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(UART_BASE + 0, divisor & 0x00FF);
    outb(UART_BASE + 1, (divisor >> 8) & 0x00FF);
    outb(UART_BASE + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(UART_BASE + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(UART_BASE + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

static int is_transmit_empty(void) {
    return inb(UART_BASE + 5) & 0x20;
}

void serial_putc(char c) {
    if (!serial_initialized) return;
    
    while (is_transmit_empty() == 0);
    outb(UART_BASE, c);
}

void serial_write_str(const char *str) {
    while (*str) {
        serial_putc(*str++);
    }
}

static int serial_8250_probe(struct device *dev __attribute__((unused))) {
    serial_configure_baud_rate(3); // 38400 baud
    serial_initialized = 1;
    
    // Test output
    serial_write_str("\n[SERIAL] 8250 UART initialized successfully at 0x3F8\n");
    
    kprintf("serial: 8250 UART initialized\n");
    return DRIVER_OK;
}

static int serial_8250_remove(struct device *dev __attribute__((unused))) {
    serial_initialized = 0;
    kprintf("serial: 8250 UART removed\n");
    return DRIVER_OK;
}

static struct driver serial_8250_driver = {
    .name = "serial_8250",
    .probe = serial_8250_probe,
    .remove = serial_8250_remove,
};

void __init serial_8250_init(void) {
    // Initialize hardware immediately even before driver registration
    // so we can use it for early logging if needed
    serial_configure_baud_rate(3); // 38400 baud
    serial_initialized = 1;
    
    driver_register(&serial_8250_driver);
}

