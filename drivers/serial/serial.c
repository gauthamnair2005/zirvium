/* drivers/serial/serial.c
 * Zirvium Kernel — 16550 UART driver
 */
#include "serial.h"
#include "arch/x64/cpu.h"
#include <stdint.h>
#include <stddef.h>

/* Register offsets from the port base */
#define UART_DATA    0   /* Transmit/Receive Holding Register */
#define UART_IER     1   /* Interrupt Enable Register */
#define UART_IIR     2   /* Interrupt Identification Register (read) */
#define UART_FCR     2   /* FIFO Control Register (write) */
#define UART_LCR     3   /* Line Control Register */
#define UART_MCR     4   /* Modem Control Register */
#define UART_LSR     5   /* Line Status Register */

/* DLAB-mode registers (LCR.DLAB = 1) */
#define UART_DLL     0   /* Divisor Latch Low  */
#define UART_DLH     1   /* Divisor Latch High */

/* Line Status Register bits */
#define LSR_DR       0x01   /* Data Ready */
#define LSR_THRE     0x20   /* Transmit Holding Register Empty */

/* Base clock rate for a PC-compatible UART */
#define UART_BASE_CLOCK  115200U

int serial_init(uint16_t port)
{
    /* Disable interrupts */
    outb((uint16_t)(port + UART_IER), 0x00);

    /* Enable DLAB to set baud divisor */
    outb((uint16_t)(port + UART_LCR), 0x80);

    /* Set divisor for 115200 baud */
    uint16_t divisor = (uint16_t)(UART_BASE_CLOCK / 115200);
    outb((uint16_t)(port + UART_DLL), (uint8_t)(divisor & 0xFF));
    outb((uint16_t)(port + UART_DLH), (uint8_t)(divisor >> 8));

    /* 8 data bits, no parity, 1 stop bit (8N1), clear DLAB */
    outb((uint16_t)(port + UART_LCR), 0x03);

    /* Enable and clear FIFO, trigger at 14 bytes */
    outb((uint16_t)(port + UART_FCR), 0xC7);

    /* RTS + DTR asserted */
    outb((uint16_t)(port + UART_MCR), 0x03);

    /* Loopback test: send 0xAE and read it back */
    outb((uint16_t)(port + UART_MCR), 0x1E);   /* loopback mode */
    outb((uint16_t)(port + UART_DATA), 0xAE);
    if (inb((uint16_t)(port + UART_DATA)) != 0xAE)
        return -1;

    /* Leave loopback, set operational mode */
    outb((uint16_t)(port + UART_MCR), 0x0F);
    return 0;
}

void serial_putc(uint16_t port, char c)
{
    /* Wait until THR is empty */
    while (!(inb((uint16_t)(port + UART_LSR)) & LSR_THRE))
        cpu_pause();
    outb(port, (uint8_t)c);
}

void serial_puts(uint16_t port, const char *s)
{
    for (; *s; s++) {
        if (*s == '\n') serial_putc(port, '\r');
        serial_putc(port, *s);
    }
}

void serial_write(uint16_t port, const char *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
        serial_putc(port, buf[i]);
}

char serial_getc(uint16_t port)
{
    while (!(inb((uint16_t)(port + UART_LSR)) & LSR_DR))
        cpu_pause();
    return (char)inb(port);
}
