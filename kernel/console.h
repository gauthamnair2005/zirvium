/* kernel/console.h
 * Zirvium Kernel — unified kernel console
 *
 * The console multiplexes output to the serial port (COM1) and, once
 * console_enable_vga() has been called, to the VGA text console as well.
 *
 * All kernel subsystems should use kputc() / kputs() rather than calling
 * serial_putc() / serial_puts() directly so that output appears both on the
 * serial line and on the physical display.
 */
#ifndef ZIRVIUM_KERNEL_CONSOLE_H
#define ZIRVIUM_KERNEL_CONSOLE_H

#include <stddef.h>

/**
 * console_init - initialise the console subsystem.
 *
 * Must be called after serial_init().  VGA output is disabled until
 * console_enable_vga() is called.
 */
void console_init(void);

/**
 * console_enable_vga - start mirroring output to the VGA text console.
 *
 * Must be called after vga_init().  From this point every kputc() / kputs()
 * call also writes to the 80×25 VGA text screen.
 */
void console_enable_vga(void);

/** kputc  - write one character to all active console backends. */
void kputc(char c);

/** kputs  - write a NUL-terminated string to all active console backends. */
void kputs(const char *s);

/** kwrite - write @len bytes from @buf to all active console backends. */
void kwrite(const char *buf, size_t len);

#endif /* ZIRVIUM_KERNEL_CONSOLE_H */
