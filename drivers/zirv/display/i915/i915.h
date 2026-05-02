/* drivers/zirv/display/i915/i915.h
 * Zirvium Kernel — Intel i915 UHD 610/620 display driver
 *
 * Supports Intel Gen 9.5 integrated graphics (Kaby Lake / Coffee Lake-U).
 * Target hardware: Intel UHD 610 (0x5902, 0x3EA0) and UHD 620 (0x5917, 0x3EA5).
 *
 * The driver:
 *  - Maps the MMIO BAR (BAR0, 16 MiB) for register access
 *  - Maps the GTT aperture (BAR2) for scanout buffers
 *  - Reads EDID via GMBUS/DDC
 *  - Performs a minimal mode-set for the eDP (internal) panel
 *  - Sets up a linear framebuffer and exposes it via /zirv/display/gpu0
 *
 * Linux reference: drivers/gpu/drm/i915/ (MIT/GPL-2.0, Intel Corporation)
 * Ported to Zirvium MOSIX APIs.
 */
#ifndef ZIRVIUM_DRIVERS_DISPLAY_I915_H
#define ZIRVIUM_DRIVERS_DISPLAY_I915_H

#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "i915_regs.h"

/* ── Framebuffer descriptor (written to /zirv/display/gpu0 device data) ──── */
typedef struct {
    uint64_t phys_addr;   /* physical address of the scanout buffer */
    void    *virt_addr;   /* virtual address (direct-mapped) */
    uint32_t width;
    uint32_t height;
    uint32_t stride;      /* bytes per line */
    uint8_t  bpp;         /* bits per pixel (32 = RGBX8888) */
} i915_framebuffer_t;

/* ── EDID (128-byte standard structure) ─────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  header[8];
    uint8_t  mfg_id[2];
    uint8_t  product_code[2];
    uint32_t serial_number;
    uint8_t  mfg_week;
    uint8_t  mfg_year;
    uint8_t  edid_version;
    uint8_t  edid_revision;
    uint8_t  video_input;
    uint8_t  hsize_cm;
    uint8_t  vsize_cm;
    uint8_t  gamma;
    uint8_t  features;
    uint8_t  chromaticity[10];
    uint8_t  timings[3];
    uint8_t  std_timing[16];
    uint8_t  dtd[4][18];   /* Detailed Timing Descriptors */
    uint8_t  ext_count;
    uint8_t  checksum;
} edid_t;

/* ── i915 device state ─────────────────────────────────────────────────────── */
typedef struct {
    pci_dev_t         *pdev;
    void              *mmio;           /* BAR0 — 16 MiB MMIO */
    void              *gtt_aperture;   /* BAR2 — GTT aperture */
    uint32_t           gtt_size;       /* bytes */
    i915_framebuffer_t fb;
    edid_t             edid;
    bool               edid_valid;
    bool               panel_on;
    bool               ready;
} i915_priv_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * i915_init - detect Intel i915 GPU on PCI and initialise the display.
 * Registers the device as /zirv/display/gpu0 and sets up a linear
 * framebuffer.  The framebuffer address is available via i915_get_fb().
 */
void i915_init(void);

/**
 * i915_get_fb - return the framebuffer descriptor (NULL if not ready).
 */
const i915_framebuffer_t *i915_get_fb(void);

/**
 * i915_set_backlight - set backlight brightness, 0–255.
 */
void i915_set_backlight(uint8_t level);

#endif /* ZIRVIUM_DRIVERS_DISPLAY_I915_H */
