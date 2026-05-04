/* drivers/vga/vga.h
 * Zirvium Kernel — VGA text-mode console driver
 *
 * Provides an 80×25 colour text console using the legacy VGA framebuffer at
 * physical address 0xB8000.  The buffer is accessible via the identity mapping
 * of the first 4 GiB that boot.asm and vmm_init both establish, so this driver
 * can be used from the very first C instruction in kernel_main().
 *
 * Character cells are 16-bit: high byte = attribute (bg<<4|fg), low byte = ASCII.
 */
#ifndef ZIRVIUM_DRIVERS_VGA_H
#define ZIRVIUM_DRIVERS_VGA_H

#include <stdint.h>
#include <stddef.h>

/* ── Geometry ─────────────────────────────────────────────────────────────── */
#define VGA_COLS   80
#define VGA_ROWS   25

/* Physical / identity-mapped address of the VGA text buffer */
#define VGA_BUFFER_PHYS  0x000B8000UL

/* ── Colour encoding ──────────────────────────────────────────────────────── */
#define VGA_COLOR(fg, bg)  ((uint8_t)(((bg) << 4) | ((fg) & 0x0F)))

typedef enum {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
} vga_color_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/** vga_init - clear the screen and reset cursor to (0,0). */
void vga_init(void);

/** vga_putc - write one character at the current cursor position. */
void vga_putc(char c);

/** vga_puts - write a NUL-terminated string. */
void vga_puts(const char *s);

/** vga_clear - fill the screen with spaces using the current colour. */
void vga_clear(void);

/** vga_set_color - change the active foreground/background attribute. */
void vga_set_color(uint8_t color);

#endif /* ZIRVIUM_DRIVERS_VGA_H */
