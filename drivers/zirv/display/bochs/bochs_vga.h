/* drivers/zirv/display/bochs/bochs_vga.h
 * Zirvium Kernel — Bochs/QEMU VGA display driver
 *
 * Supports the Bochs VBE display device that QEMU emulates for the standard
 * VGA adapter (PCI 0x1234:0x1111) and the dedicated Bochs display device
 * (PCI 0x1234:0x2111, "-device bochs-display").
 *
 * The Bochs VBE interface exposes a 16-bit I/O register file at ports
 * 0x01CE (index) and 0x01CF (data).  Writing XRES, YRES, BPP, and ENABLE
 * configures a linear framebuffer whose base address is in BAR0.
 *
 * The driver sets a 1024×768 32-bpp mode, maps BAR0 into kernel virtual
 * memory, and registers the device under /zirv/display/gpu0.
 *
 * Reference: https://github.com/qemu/qemu (hw/display/bochs-display.c)
 *            https://wiki.osdev.org/Bochs_VBE_Extensions
 */
#ifndef ZIRVIUM_DRIVERS_DISPLAY_BOCHS_VGA_H
#define ZIRVIUM_DRIVERS_DISPLAY_BOCHS_VGA_H

#include <stdint.h>
#include <stdbool.h>

/* ── Bochs VBE register indices (written to port 0x01CE) ─────────────────── */
#define VBE_DISPI_INDEX_ID          0x0u
#define VBE_DISPI_INDEX_XRES        0x1u
#define VBE_DISPI_INDEX_YRES        0x2u
#define VBE_DISPI_INDEX_BPP         0x3u
#define VBE_DISPI_INDEX_ENABLE      0x4u
#define VBE_DISPI_INDEX_BANK        0x5u
#define VBE_DISPI_INDEX_VIRT_WIDTH  0x6u
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x7u
#define VBE_DISPI_INDEX_X_OFFSET    0x8u
#define VBE_DISPI_INDEX_Y_OFFSET    0x9u

/* ── VBE_DISPI_INDEX_ENABLE flags ────────────────────────────────────────── */
#define VBE_DISPI_DISABLED    0x0000u
#define VBE_DISPI_ENABLED     0x0001u
#define VBE_DISPI_LFB_ENABLED 0x0040u   /* linear framebuffer */

/* ── I/O ports for the Bochs VBE register file ───────────────────────────── */
#define VBE_DISPI_IOPORT_INDEX  0x01CEu
#define VBE_DISPI_IOPORT_DATA   0x01CFu

/* ── PCI identifiers ──────────────────────────────────────────────────────── */
#define BOCHS_VGA_VENDOR_ID   0x1234u
#define BOCHS_VGA_DEVICE_STD  0x1111u   /* QEMU stdvga / VGA compatible */
#define BOCHS_VGA_DEVICE_BX   0x2111u   /* QEMU bochs-display device    */

/* ── Default framebuffer geometry ────────────────────────────────────────── */
#define BOCHS_VGA_WIDTH   1024u
#define BOCHS_VGA_HEIGHT   768u
#define BOCHS_VGA_BPP       32u

/* ── Framebuffer descriptor ──────────────────────────────────────────────── */
typedef struct {
    void    *fb_virt;   /* kernel virtual address of the linear framebuffer */
    uint64_t fb_phys;   /* physical address (BAR0) */
    uint32_t width;
    uint32_t height;
    uint32_t stride;    /* bytes per row */
    uint8_t  bpp;
    bool     ready;
} bochs_vga_fb_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * bochs_vga_init - probe PCI for the Bochs VBE device and initialise it.
 *
 * Configures a 1024×768 32-bpp linear framebuffer via the Bochs VBE I/O
 * registers, maps BAR0 into virtual memory, clears the screen to black, and
 * registers the device as /zirv/display/gpu0.
 *
 * Must be called after pci_init() and vmm_init().
 */
#include "drivers/zirv/driver.h"

extern const zirv_driver_t g_bochs_vga_driver;

void bochs_vga_init(void);

/**
 * bochs_vga_get_fb - return the framebuffer descriptor, or NULL if not ready.
 */
const bochs_vga_fb_t *bochs_vga_get_fb(void);

#endif /* ZIRVIUM_DRIVERS_DISPLAY_BOCHS_VGA_H */
