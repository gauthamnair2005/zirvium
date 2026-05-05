/* drivers/vga/fb_console.h
 * Zirvium Kernel — pixel-framebuffer text console
 *
 * Renders kernel console output as 8×16 character glyphs directly into a
 * 32-bpp linear framebuffer.  Used when a GPU driver (bochs_vga, i915, …)
 * has switched the display to a pixel mode, making the legacy VGA text
 * buffer at 0xB8000 invisible to the user.
 *
 * Colour format: 0x00RRGGBB (red in bits 16-23, green in bits 8-15,
 * blue in bits 0-7) — this matches the little-endian BGRX byte order used
 * by both the Bochs/QEMU VBE device and the Intel i915 scanout buffer.
 */
#ifndef ZIRVIUM_DRIVERS_VGA_FB_CONSOLE_H
#define ZIRVIUM_DRIVERS_VGA_FB_CONSOLE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Character cell size (must match the embedded font) */
#define FB_FONT_W   8u    /* pixels wide */
#define FB_FONT_H  16u    /* pixels tall */

/* Default colour scheme: light-grey text on black, matching VGA text mode */
#define FB_COL_FG  0x00C0C0C0u   /* light grey   */
#define FB_COL_BG  0x00000000u   /* black        */

/**
 * fb_console_init - attach the console to a linear framebuffer.
 *
 * @fb:     kernel virtual address of the first pixel
 * @width:  framebuffer width in pixels
 * @height: framebuffer height in pixels
 * @stride: bytes per pixel row (may be > width * 4 when there is padding)
 * @bpp:    bits per pixel; only 32 is supported at this time
 *
 * Clears the screen and positions the cursor at (0, 0).
 * Returns true on success, false if the parameters are unsupported.
 */
bool fb_console_init(void *fb, uint32_t width, uint32_t height,
                     uint32_t stride, uint8_t bpp);

/**
 * fb_console_putc - write one character at the current cursor position.
 *
 * Handles '\n' (newline), '\r' (carriage return), '\t' (tab to next
 * 8-column boundary), and '\b' (backspace, erase the previous cell).
 * Scrolls the visible area up by one line when the last row is full.
 */
void fb_console_putc(char c);

/**
 * fb_console_puts - write a NUL-terminated string.
 */
void fb_console_puts(const char *s);

/**
 * fb_console_ready - returns true once fb_console_init() has succeeded.
 */
bool fb_console_ready(void);

#endif /* ZIRVIUM_DRIVERS_VGA_FB_CONSOLE_H */
