/* kernel/console.c
 * Zirvium Kernel — unified kernel console implementation
 *
 * Forwards every character to:
 *  - The serial port (COM1) — always active for debugging / QEMU -serial stdio
 *  - The VGA text console (0xB8000) — once console_enable_vga() has been called
 *  - A pixel framebuffer — once console_enable_fb() has been called
 *
 * The pixel-framebuffer backend keeps boot messages visible in the QEMU SDL
 * window after bochs_vga_init() or i915_init() switches the display from VGA
 * text mode to a native pixel mode.
 */
#include "console.h"
#include "drivers/serial/serial.h"
#include "drivers/vga/vga.h"
#include "drivers/vga/fb_console.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

static bool g_vga_active = false;

void console_init(void)
{
    /* Serial is already running; VGA and FB are enabled separately. */
    g_vga_active = false;
}

void console_enable_vga(void)
{
    g_vga_active = true;
}

void console_enable_fb(void *fb, uint32_t width, uint32_t height,
                       uint32_t stride, uint8_t bpp)
{
    fb_console_init(fb, width, height, stride, bpp);
}

void kputc(char c)
{
    serial_putc(SERIAL_COM1, c);
    if (g_vga_active)
        vga_putc(c);
    if (fb_console_ready())
        fb_console_putc(c);
}

void kputs(const char *s)
{
    serial_puts(SERIAL_COM1, s);
    if (g_vga_active)
        vga_puts(s);
    if (fb_console_ready())
        fb_console_puts(s);
}

void kwrite(const char *buf, size_t len)
{
    serial_write(SERIAL_COM1, buf, len);
    if (g_vga_active) {
        for (size_t i = 0; i < len; i++)
            vga_putc(buf[i]);
    }
    if (fb_console_ready()) {
        for (size_t i = 0; i < len; i++)
            fb_console_putc(buf[i]);
    }
}

/* ── kprintf — formatted output ──────────────────────────────────────────── */
static void kprintf_itoa(uint64_t n, char *s, int base)
{
    static const char digits[] = "0123456789abcdef";
    char buf[65];
    int i = 0;
    if (n == 0) { s[0] = '0'; s[1] = '\0'; return; }
    while (n > 0) { buf[i++] = digits[n % (unsigned)base]; n /= (unsigned)base; }
    for (int j = 0; j < i; j++) s[j] = buf[i - j - 1];
    s[i] = '\0';
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { kputc(*p); continue; }
        p++;
        switch (*p) {
        case 's': {
            const char *s = va_arg(args, const char *);
            kputs(s ? s : "(null)");
            break;
        }
        case 'd': {
            int n = va_arg(args, int);
            uint32_t u;
            if (n < 0) { kputc('-'); u = (n == -2147483647 - 1) ? 2147483648u : (uint32_t)-n; }
            else { u = (uint32_t)n; }
            char buf[32]; kprintf_itoa(u, buf, 10); kputs(buf);
            break;
        }
        case 'u': {
            char buf[32]; kprintf_itoa(va_arg(args, uint32_t), buf, 10); kputs(buf);
            break;
        }
        case 'x': {
            char buf[32]; kprintf_itoa(va_arg(args, uint32_t), buf, 16); kputs(buf);
            break;
        }
        case 'p': {
            kputs("0x");
            char buf[65]; kprintf_itoa((uint64_t)(uintptr_t)va_arg(args, void *), buf, 16); kputs(buf);
            break;
        }
        case 'c':
            kputc((char)va_arg(args, int));
            break;
        case '%':
            kputc('%');
            break;
        default:
            kputc('%'); kputc(*p);
            break;
        }
    }

    va_end(args);
}
