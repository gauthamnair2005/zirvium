/* kernel/console.h
 * Zirvium Kernel — unified kernel console
 *
 * The console multiplexes output to:
 *  - The serial port (COM1) — always active.
 *  - The VGA text console (0xB8000) — after console_enable_vga().
 *  - A pixel framebuffer — after console_enable_fb().
 *
 * The pixel-framebuffer backend uses drivers/vga/fb_console to render text
 * glyphs.  It is activated by GPU drivers (bochs_vga, i915, …) once their
 * linear framebuffer is ready, so that boot messages remain visible in the
 * QEMU SDL window even after the display switches from VGA text mode to the
 * VBE / native pixel mode.
 *
 * All kernel subsystems should use kputc() / kputs() / kprintf() rather
 * than calling serial_putc() / serial_puts() directly.
 */
#ifndef ZIRVIUM_KERNEL_CONSOLE_H
#define ZIRVIUM_KERNEL_CONSOLE_H

#include <stddef.h>
#include <stdint.h>

/**
 * console_init - initialise the console subsystem.
 *
 * Must be called after serial_init().  VGA and FB output are disabled until
 * their respective enable calls are made.
 */
void console_init(void);

/**
 * console_enable_vga - start mirroring output to the VGA text console.
 *
 * Must be called after vga_init().  From this point every kputc() / kputs()
 * call also writes to the 80×25 VGA text screen.
 */
void console_enable_vga(void);

/**
 * console_enable_fb - start mirroring output to a pixel framebuffer.
 *
 * @fb:     kernel virtual address of the linear framebuffer
 * @width:  framebuffer width in pixels
 * @height: framebuffer height in pixels
 * @stride: bytes per pixel row
 * @bpp:    bits per pixel (32 supported)
 *
 * Must be called after the GPU driver has mapped and configured its
 * linear framebuffer.  From this point every kputc() / kputs() call also
 * renders a glyph on the pixel display so that status messages remain
 * visible when the display is in a pixel (VBE / native GPU) mode.
 *
 * Re-plays any text that was written before fb was enabled so that earlier
 * boot messages appear on screen.
 */
void console_enable_fb(void *fb, uint32_t width, uint32_t height,
                       uint32_t stride, uint8_t bpp);

/** kputc  - write one character to all active console backends. */
void kputc(char c);

/** kputs  - write a NUL-terminated string to all active console backends. */
void kputs(const char *s);

/** kwrite - write @len bytes from @buf to all active console backends. */
void kwrite(const char *buf, size_t len);

/** kprintf - formatted output to all active console backends (%s %d %u %x %p %c). */
void kprintf(const char *fmt, ...);

#endif /* ZIRVIUM_KERNEL_CONSOLE_H */
