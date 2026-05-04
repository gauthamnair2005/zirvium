/* kernel/console.c
 * Zirvium Kernel — unified kernel console implementation
 *
 * Forwards every character to:
 *  - The serial port (always, for remote debugging / QEMU -serial stdio)
 *  - The VGA text console (once console_enable_vga() has been called)
 */
#include "console.h"
#include "drivers/serial/serial.h"
#include "drivers/vga/vga.h"
#include <stdbool.h>
#include <stddef.h>

static bool g_vga_active = false;

void console_init(void)
{
    /* Serial is already running; VGA is enabled separately. */
    g_vga_active = false;
}

void console_enable_vga(void)
{
    g_vga_active = true;
}

void kputc(char c)
{
    serial_putc(SERIAL_COM1, c);
    if (g_vga_active)
        vga_putc(c);
}

void kputs(const char *s)
{
    serial_puts(SERIAL_COM1, s);
    if (g_vga_active)
        vga_puts(s);
}

void kwrite(const char *buf, size_t len)
{
    serial_write(SERIAL_COM1, buf, len);
    if (g_vga_active) {
        for (size_t i = 0; i < len; i++)
            vga_putc(buf[i]);
    }
}
