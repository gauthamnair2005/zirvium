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
#include "kernel/syscall/syscall.h"
#include "drivers/serial/serial.h"
#include "drivers/vga/vga.h"
#include "drivers/vga/fb_console.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

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
static void kvprintf(const char *fmt, va_list args)
{
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    kputs(buf);
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}

/* ── Colour support ───────────────────────────────────────────────────────── */

/* ANSI SGR escape sequences emitted on the serial port */
static const char *const g_ansi_color[] = {
    [CONSOLE_COLOR_DEFAULT] = "\033[0m",
    [CONSOLE_COLOR_GREEN]   = "\033[32m",
    [CONSOLE_COLOR_RED]     = "\033[31m",
    [CONSOLE_COLOR_YELLOW]  = "\033[33m",
    [4]                     = "\033[36m", /* cyan for info */
    [5]                     = "\033[90m", /* bright black (grey) for debug */
};

/* VGA text-mode attributes (fg on black background) */
static const uint8_t g_vga_color[] = {
    [CONSOLE_COLOR_DEFAULT] = VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK),
    [CONSOLE_COLOR_GREEN]   = VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK),
    [CONSOLE_COLOR_RED]     = VGA_COLOR(VGA_LIGHT_RED, VGA_BLACK),
    [CONSOLE_COLOR_YELLOW]  = VGA_COLOR(VGA_YELLOW, VGA_BLACK),
    [4]                     = VGA_COLOR(VGA_CYAN, VGA_BLACK),
    [5]                     = VGA_COLOR(VGA_DARK_GREY, VGA_BLACK),
};

/* Pixel framebuffer colours (0x00RRGGBB) */
static const uint32_t g_fb_color[] = {
    [CONSOLE_COLOR_DEFAULT] = 0x00C0C0C0u,   /* light grey  */
    [CONSOLE_COLOR_GREEN]   = 0x0000C000u,   /* green       */
    [CONSOLE_COLOR_RED]     = 0x00C00000u,   /* red         */
    [CONSOLE_COLOR_YELLOW]  = 0x00C0C000u,   /* yellow      */
    [4]                     = 0x0000C0C0u,   /* cyan        */
    [5]                     = 0x00808080u,   /* dark grey   */
};

void console_set_color(console_color_t color)
{
    if ((unsigned)color >= 6u)
        color = CONSOLE_COLOR_DEFAULT;

    /* Serial: ANSI escape sequence */
    serial_puts(SERIAL_COM1, g_ansi_color[color]);

    /* VGA text console */
    if (g_vga_active)
        vga_set_color(g_vga_color[color]);

    /* Pixel framebuffer */
    if (fb_console_ready())
        fb_console_set_fg_color(g_fb_color[color]);
}

void console_reset_color(void)
{
    console_set_color(CONSOLE_COLOR_DEFAULT);
}

/* ── Console ioctl — terminal control from userspace ──────────────────────── */
int console_ioctl(int cmd, uint64_t arg)
{
    switch (cmd) {
    case TC_CLEAR:
        if (g_vga_active) vga_clear();
        if (fb_console_ready()) fb_console_clear();
        return 0;

    case TC_SET_CURSOR: {
        int row = (int)(arg >> 16) & 0xFFFF;
        int col = (int)(arg & 0xFFFF);
        if (g_vga_active) vga_set_cursor(row, col);
        if (fb_console_ready()) fb_console_set_cursor((uint32_t)col, (uint32_t)row);
        return 0;
    }

    case TC_GET_CURSOR: {
        int row = 0, col = 0;
        if (g_vga_active) {
            row = vga_get_cursor_row();
            col = vga_get_cursor_col();
        } else if (fb_console_ready()) {
            row = (int)fb_console_get_cursor_row();
            col = (int)fb_console_get_cursor_col();
        }
        return (row << 16) | (col & 0xFFFF);
    }

    case TC_SET_COLOR: {
        uint8_t fg = (uint8_t)(arg >> 16) & 0x0F;
        uint8_t bg = (uint8_t)(arg >> 24) & 0x0F;
        if (g_vga_active) vga_set_color(VGA_COLOR(fg, bg));
        if (fb_console_ready()) {
            /* Map VGA colour index to 0x00RRGGBB using the fb color table */
            static const uint32_t fb_palette[16] = {
                0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
                0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
                0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
                0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF,
            };
            if ((unsigned)fg < 16)
                fb_console_set_fg_color(fb_palette[fg]);
        }
        return 0;
    }

    case TC_GET_SIZE: {
        int rows = 0, cols = 0;
        if (g_vga_active) {
            rows = vga_get_rows();
            cols = vga_get_cols();
        } else if (fb_console_ready()) {
            rows = (int)fb_console_get_rows();
            cols = (int)fb_console_get_cols();
        }
        return (rows << 16) | (cols & 0xFFFF);
    }

    case TC_SCROLL:
        if (g_vga_active) vga_scroll((int)arg);
        if (fb_console_ready()) fb_console_scroll((int)arg);
        return 0;

    default:
        return -1;
    }
}

/* ── klog implementation ─────────────────────────────────────────────────── */

void klog(log_level_t level, const char *component, const char *fmt, ...)
{
    /* Format: [ 000000 ] [  OK  ] [ COMP ] Message */
    kputs("[ 000000 ] "); /* TODO: add uptime counter */

    switch (level) {
    case LOG_OK:
        console_set_color(CONSOLE_COLOR_GREEN);
        kputs("[  OK  ] ");
        break;
    case LOG_INFO:
        console_set_color((console_color_t)4);
        kputs("[ INFO ] ");
        break;
    case LOG_WARN:
        console_set_color(CONSOLE_COLOR_YELLOW);
        kputs("[ WARN ] ");
        break;
    case LOG_FAIL:
        console_set_color(CONSOLE_COLOR_RED);
        kputs("[ FAIL ] ");
        break;
    case LOG_DEBUG:
        console_set_color((console_color_t)5);
        kputs("[DEBUG ] ");
        break;
    }
    console_reset_color();

    /* Component tag */
    kputs("[ ");
    if (component) {
        int len = 0;
        while (component[len] && len < 4) { kputc(component[len]); len++; }
        while (len < 4) { kputc(' '); len++; }
    } else {
        kputs("    ");
    }
    kputs(" ] ");

    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);

    kputc('\n');
}

void kprint_ok(void)   { klog(LOG_OK,   "INIT", "Success"); }
void kprint_fail(void) { klog(LOG_FAIL, "INIT", "Failure"); }
void kprint_warn(void) { klog(LOG_WARN, "INIT", "Warning"); }
