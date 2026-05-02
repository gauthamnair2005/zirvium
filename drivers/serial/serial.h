/* drivers/serial/serial.h
 * Zirvium Kernel — Serial (UART 16550) driver
 */
#ifndef ZIRVIUM_DRIVERS_SERIAL_H
#define ZIRVIUM_DRIVERS_SERIAL_H

#include <stdint.h>
#include <stddef.h>

/* COM port base addresses */
#define SERIAL_COM1  0x3F8
#define SERIAL_COM2  0x2F8
#define SERIAL_COM3  0x3E8
#define SERIAL_COM4  0x2E8

/**
 * serial_init - initialise a UART at @port with 115200 baud, 8N1.
 * Returns 0 on success, -1 if the port does not appear to be present.
 */
int serial_init(uint16_t port);

/** serial_putc - write one character (blocking). */
void serial_putc(uint16_t port, char c);

/** serial_puts - write a NUL-terminated string. */
void serial_puts(uint16_t port, const char *s);

/** serial_write - write @len bytes from @buf. */
void serial_write(uint16_t port, const char *buf, size_t len);

/** serial_getc - read one character (blocking). */
char serial_getc(uint16_t port);

#endif /* ZIRVIUM_DRIVERS_SERIAL_H */
