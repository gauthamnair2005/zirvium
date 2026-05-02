/* lib/stdio.c
 * Zirvium Kernel — simple I/O functions
 */
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include "drivers/serial/serial.h"

static void itoa(uint64_t n, char *s, int base) {
    static const char digits[] = "0123456789abcdef";
    char buf[65];
    int i = 0;
    if (n == 0) {
        s[0] = '0';
        s[1] = '\0';
        return;
    }
    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    for (int j = 0; j < i; j++) {
        s[j] = buf[i - j - 1];
    }
    s[i] = '\0';
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            serial_putc(SERIAL_COM1, *p);
            continue;
        }

        p++;
        if (*p == 's') {
            const char *s = va_arg(args, const char *);
            serial_puts(SERIAL_COM1, s ? s : "(null)");
        } else if (*p == 'd') {
            int n = va_arg(args, int);
            if (n < 0) {
                serial_putc(SERIAL_COM1, '-');
                n = -n;
            }
            char buf[32];
            itoa((uint32_t)n, buf, 10);
            serial_puts(SERIAL_COM1, buf);
        } else if (*p == 'u') {
            uint32_t n = va_arg(args, uint32_t);
            char buf[32];
            itoa(n, buf, 10);
            serial_puts(SERIAL_COM1, buf);
        } else if (*p == 'x') {
            uint32_t n = va_arg(args, uint32_t);
            char buf[32];
            itoa(n, buf, 16);
            serial_puts(SERIAL_COM1, buf);
        } else if (*p == 'p') {
            uint64_t n = (uint64_t)va_arg(args, void *);
            serial_puts(SERIAL_COM1, "0x");
            char buf[65];
            itoa(n, buf, 16);
            serial_puts(SERIAL_COM1, buf);
        } else if (*p == 'c') {
            char c = (char)va_arg(args, int);
            serial_putc(SERIAL_COM1, c);
        } else if (*p == '%') {
            serial_putc(SERIAL_COM1, '%');
        }
    }

    va_end(args);
}
