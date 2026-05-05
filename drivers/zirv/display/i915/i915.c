/* drivers/zirv/display/i915/i915.c
 * Zirvium Kernel — Intel i915 UHD 610/620 display driver
 *
 * Ported from Linux drivers/gpu/drm/i915/ (MIT/GPL-2.0, Intel Corp.)
 * Re-implemented using Zirvium MOSIX APIs.
 *
 * This file handles:
 *  1. PCI probe and MMIO / GTT aperture mapping
 *  2. GMBUS EDID read from the internal eDP panel
 *  3. Minimal mode-set sequence for eDP
 *  4. Scanout framebuffer allocation and plane setup
 *  5. Backlight PWM control
 */
#include "i915.h"
#include "i915_regs.h"
#include "drivers/pci/pci.h"
#include "drivers/zirv/device.h"
#include "kernel/console.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "drivers/compat/linux_compat.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Static state ─────────────────────────────────────────────────────────── */
static i915_priv_t g_i915;
static bool g_i915_found = false;

/* ── Supported PCI devices ────────────────────────────────────────────────── */
static const uint16_t i915_device_ids[] = {
    /* Gen9.5 — Kaby Lake / Coffee Lake */
    INTEL_KBL_GT1_ULT,   /* UHD 610 (Kaby Lake) */
    INTEL_KBL_GT1_ULX,   /* UHD 610 (Kaby Lake X) */
    INTEL_KBLR_GT2,      /* UHD 620 (Kaby Lake-R) */
    INTEL_CFL_GT1_ULT,   /* UHD 610 (Coffee Lake) */
    INTEL_CFL_GT2_ULT,   /* UHD 620 (Coffee Lake) */
    0x3E92,              /* UHD 630 (Coffee Lake DT) */
    0x3E9B,              /* UHD 630 (Coffee Lake Mobile) */
    /* Gen11 — Ice Lake */
    INTEL_ICL_GT2_ULT,   /* Iris Plus G7 80EU */
    INTEL_ICL_GT2_ULT_1, /* Iris Plus G4 */
    INTEL_ICL_GT2_ULT_2, /* Iris Plus G4 */
    INTEL_ICL_GT2_ULT_3, /* Iris Plus G4 */
    INTEL_ICL_GT2_ULT_4, /* Iris Plus G7 */
    INTEL_ICL_GT1_ULT,   /* UHD Graphics */
    INTEL_ICL_GT05_ULT,  /* UHD Graphics 32EU */
    INTEL_ICL_GT05_ULT_1,/* Iris Plus G1 */
    INTEL_ICL_GT05_ULT_2,/* UHD Graphics */
    /* Gen12 — Tiger Lake */
    INTEL_TGL_GT2_U,     /* Iris Xe G7 96EU */
    INTEL_TGL_GT2_U_1,   /* Iris Xe G7 80EU */
    INTEL_TGL_GT2_U_2,   /* Iris Xe */
    INTEL_TGL_GT1_U,     /* UHD Graphics */
    INTEL_TGL_GT1_U_1,   /* UHD Graphics */
    INTEL_TGL_GT1_U_2,   /* UHD Graphics */
    INTEL_TGL_GT1_U_3,   /* UHD Graphics */
    /* Gen12.2 — Alder Lake */
    INTEL_ADL_S_GT1,     /* UHD 730 */
    INTEL_ADL_S_GT1_1,   /* UHD 730 */
    INTEL_ADL_S_GT2,     /* UHD 770 */
    INTEL_ADL_P_GT2,     /* Iris Xe */
    INTEL_ADL_P_GT2_1,   /* Iris Xe */
    INTEL_ADL_P_GT2_2,   /* Iris Xe */
    INTEL_ADL_P_GT1,     /* UHD Graphics */
    INTEL_ADL_N_GT1,     /* UHD Graphics */
    INTEL_ADL_N_GT1_1,   /* UHD Graphics */
    INTEL_ADL_N_GT1_2,   /* UHD Graphics */
    /* Gen12.2 — Raptor Lake */
    INTEL_RPL_S_GT1,     /* UHD 730 */
    INTEL_RPL_S_GT2,     /* UHD 770 */
    INTEL_RPL_P_GT2,     /* Iris Xe */
    INTEL_RPL_P_GT2_1,   /* Iris Xe */
    INTEL_RPL_P_GT1,     /* UHD Graphics */
    /* Gen14 — Meteor Lake */
    INTEL_MTL_M_GT2,     /* Arc Graphics */
    INTEL_MTL_P_GT2,     /* Graphics */
    INTEL_MTL_P_GT2_1,   /* Arc Graphics */
};

/* ── MMIO helpers ─────────────────────────────────────────────────────────── */
#define I915_READ(reg)      readl((uint8_t *)g_i915.mmio + (reg))
#define I915_WRITE(reg,val) writel((val), (uint8_t *)g_i915.mmio + (reg))

/* ── Wait for a masked register value ───────────────────────────────────── */
static bool i915_wait_for(uint32_t reg, uint32_t mask, uint32_t val,
                           unsigned int timeout_ms)
{
    for (unsigned int i = 0; i < timeout_ms * 10; i++) {
        if ((I915_READ(reg) & mask) == val) return true;
        udelay(100);
    }
    return false;
}

/* ── GMBUS (I2C DDC for EDID) ────────────────────────────────────────────── */
#define GMBUS_PIN_EDP_BKLCTL  2   /* eDP AUX / panel DDC */

static bool gmbus_wait_hw_rdy(void)
{
    return i915_wait_for(GMBUS2, GMBUS_HW_RDY, GMBUS_HW_RDY, 50);
}

static bool i915_read_edid(edid_t *edid)
{
    /* Select DDC pin for internal eDP panel */
    I915_WRITE(GMBUS0, GMBUS_PIN_EDP_BKLCTL);
    udelay(10);

    /* Clear any previous status */
    I915_WRITE(GMBUS1, GMBUS_SW_CLR_INT);
    I915_WRITE(GMBUS1, 0);

    /* Issue a "read 128 bytes from slave 0x50 (EDID)" request */
    /* GMBUS1: SW_RDY | CYCLE_INDEX | byte_count | slave_addr | READ */
    uint32_t gmbus1_val =
          GMBUS_SW_RDY
        | (3u << 25)                    /* WAIT cycle */
        | ((128u) << GMBUS_BYTE_CNT_SHIFT)
        | ((0x50u) << GMBUS_SLAVE_ADDR_SHIFT)
        | 1u;                           /* READ direction */
    I915_WRITE(GMBUS1, gmbus1_val);

    uint8_t *out = (uint8_t *)edid;
    for (int word = 0; word < 32; word++) {
        if (!gmbus_wait_hw_rdy()) {
            kputs("[i915] GMBUS timeout reading EDID\n");
            /* Clear GMBUS */
            I915_WRITE(GMBUS1, GMBUS_SW_CLR_INT);
            return false;
        }
        uint32_t data = I915_READ(GMBUS3);
        out[word * 4 + 0] = (uint8_t)( data        & 0xFF);
        out[word * 4 + 1] = (uint8_t)((data >>  8) & 0xFF);
        out[word * 4 + 2] = (uint8_t)((data >> 16) & 0xFF);
        out[word * 4 + 3] = (uint8_t)((data >> 24) & 0xFF);
    }

    /* Stop sequence */
    I915_WRITE(GMBUS1, GMBUS_SW_CLR_INT);
    I915_WRITE(GMBUS0, 0);

    /* Validate EDID header: 00 FF FF FF FF FF FF 00 */
    const uint8_t magic[8] = {0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00};
    return memcmp(edid->header, magic, 8) == 0;
}

/* ── Panel power on/off ───────────────────────────────────────────────────── */
static void i915_panel_power_on(void)
{
    uint32_t pp = I915_READ(PP_CONTROL);
    pp |= PANEL_POWER_ON;
    I915_WRITE(PP_CONTROL, pp);

    /* Wait for PP_STATUS.PP_ON to set */
    i915_wait_for(PP_STATUS, PP_ON, PP_ON, 500);
    g_i915.panel_on = true;
}

static void i915_panel_power_off(void)
{
    uint32_t pp = I915_READ(PP_CONTROL);
    pp &= ~(uint32_t)PANEL_POWER_ON;
    I915_WRITE(PP_CONTROL, pp);
    i915_wait_for(PP_STATUS, PP_ON, 0, 500);
    g_i915.panel_on = false;
}

/* ── Parse preferred mode from EDID DTD[0] ─────────────────────────────── */
static void i915_parse_edid_mode(uint32_t *width_out, uint32_t *height_out)
{
    /* DTD[0] bytes:
     *   [0:1] pixel clock / 10 kHz
     *   [2]   H active pixels [7:0]
     *   [3]   H blanking [7:0]
     *   [4]   H active pixels [11:8] | H blanking [11:8]
     *   [5]   V active lines [7:0]
     *   [6]   V blanking [7:0]
     *   [7]   V active lines [11:8] | V blanking [11:8]
     */
    const uint8_t *dtd = g_i915.edid.dtd[0];
    uint32_t h_active = (uint32_t)dtd[2]
                      | (((uint32_t)dtd[4] >> 4) << 8);
    uint32_t v_active = (uint32_t)dtd[5]
                      | (((uint32_t)dtd[7] >> 4) << 8);
    *width_out  = h_active;
    *height_out = v_active;
}

/* ── Framebuffer allocation ───────────────────────────────────────────────── */
static bool i915_alloc_fb(uint32_t width, uint32_t height)
{
    uint32_t stride = width * 4;   /* 32 bpp */
    size_t   pages  = (stride * height + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t phys   = pmm_alloc_pages(pages);
    if (!phys) return false;

    g_i915.fb.phys_addr = phys;
    g_i915.fb.virt_addr = PHYS_TO_VIRT(phys);
    g_i915.fb.width     = width;
    g_i915.fb.height    = height;
    g_i915.fb.stride    = stride;
    g_i915.fb.bpp       = 32;

    /* Clear to black */
    memset(g_i915.fb.virt_addr, 0, stride * height);
    return true;
}

/* ── Mode-set sequence for eDP pipe A ───────────────────────────────────── */
static void i915_modeset(void)
{
    uint32_t w = g_i915.fb.width;
    uint32_t h = g_i915.fb.height;

    /* Program pipe timing (simplified; use EDID-derived values in a full impl) */
    uint32_t htotal = (w - 1) | ((w + 80 - 1) << 16);    /* active | total */
    uint32_t vtotal = (h - 1) | ((h + 30 - 1) << 16);
    I915_WRITE(HTOTAL(PIPE_A), htotal);
    I915_WRITE(HBLANK(PIPE_A), ((w - 1) << 16) | (w + 80 - 1));
    I915_WRITE(HSYNC(PIPE_A),  ((w + 8 - 1) << 16) | (w + 40 - 1));
    I915_WRITE(VTOTAL(PIPE_A), vtotal);
    I915_WRITE(VBLANK(PIPE_A), ((h - 1) << 16) | (h + 30 - 1));
    I915_WRITE(VSYNC(PIPE_A),  ((h + 2 - 1) << 16) | (h + 8 - 1));

    /* Enable pipe A */
    I915_WRITE(PIPECONF(PIPE_A),
               PIPECONF_ENABLE | PIPECONF_BPC_8);

    /* Set up primary plane A */
    I915_WRITE(DSPSTRIDE(PIPE_A), g_i915.fb.stride);
    I915_WRITE(DSPLINOFF(PIPE_A), 0);
    I915_WRITE(DSPTILEOFF(PIPE_A), 0);
    /* Set framebuffer base — address relative to GTT aperture start */
    I915_WRITE(DSPSURF(PIPE_A), (uint32_t)(g_i915.fb.phys_addr & 0xFFFFFFFF));

    I915_WRITE(DSPCNTR(PIPE_A),
               DISPLAY_PLANE_ENABLE | DISPPLANE_RGBX888);

    mb();
}

/* ── Backlight control ───────────────────────────────────────────────────── */
static uint32_t g_blc_max = 0;

static void i915_backlight_init(void)
{
    uint32_t ctl = I915_READ(BLC_PWM_CPU_CTL2);
    if (!(ctl & (1u << 31))) {
        /* Enable backlight controller */
        I915_WRITE(BLC_PWM_CPU_CTL2, ctl | (1u << 31));
    }
    /* Read current max duty cycle from CPU PWM register */
    g_blc_max = (I915_READ(BLC_PWM_CPU_CTL) >> BACKLIGHT_DUTY_CYCLE_SHIFT)
                & 0xFFFF;
    if (!g_blc_max) g_blc_max = 0xFF;
}

void i915_set_backlight(uint8_t level)
{
    if (!g_i915_found || !g_blc_max) return;
    uint32_t duty = ((uint32_t)level * g_blc_max) / 255u;
    uint32_t ctl  = I915_READ(BLC_PWM_CPU_CTL);
    ctl = (ctl & 0x0000FFFF) | (duty << BACKLIGHT_DUTY_CYCLE_SHIFT);
    I915_WRITE(BLC_PWM_CPU_CTL, ctl);
}

/* ── Public API ───────────────────────────────────────────────────────────── */
void i915_init(void)
{
    kputs("[i915] Scanning PCI for Intel GPU\n");

    pci_dev_t *pdev = NULL;
    for (size_t k = 0; k < sizeof(i915_device_ids)/sizeof(i915_device_ids[0]); k++) {
        pdev = pci_find_device(PCI_VENDOR_INTEL, i915_device_ids[k]);
        if (pdev) break;
    }
    if (!pdev) {
        kputs("[i915] Intel GPU not found\n");
        return;
    }

    kputs("[i915] Intel integrated GPU found\n");
    pci_enable_device(pdev);

    g_i915.pdev = pdev;

    /* Map BAR0 (16 MiB MMIO register space) */
    g_i915.mmio = pci_map_bar(pdev, 0);
    if (!g_i915.mmio) {
        kputs("[i915] Failed to map BAR0 (MMIO)\n");
        return;
    }

    /* Map BAR2 (GTT aperture) */
    g_i915.gtt_aperture = pci_map_bar(pdev, 2);
    if (g_i915.gtt_aperture)
        g_i915.gtt_size = (uint32_t)pdev->bars[2].size;

    /* Try to read EDID from internal panel */
    if (i915_read_edid(&g_i915.edid)) {
        g_i915.edid_valid = true;
        kputs("[i915] EDID read successfully\n");
    } else {
        kputs("[i915] EDID read failed, using 1366x768\n");
    }

    /* Determine display resolution */
    uint32_t width = 1366, height = 768;   /* safe default */
    if (g_i915.edid_valid)
        i915_parse_edid_mode(&width, &height);

    /* Allocate framebuffer */
    if (!i915_alloc_fb(width, height)) {
        kputs("[i915] FB allocation failed\n");
        return;
    }

    /* Power on panel, run mode-set, enable backlight */
    i915_panel_power_on();
    i915_modeset();
    i915_backlight_init();
    i915_set_backlight(200);   /* ~78% brightness */

    g_i915.ready = true;
    g_i915_found = true;

    /* Register with /zirv/display/gpu0 */
    device_desc_t *desc = (device_desc_t *)kzalloc(sizeof(device_desc_t), 0);
    if (desc) {
        desc->bus_class   = DEV_CLASS_DISPLAY_GPU;
        desc->media_class = DEV_CLASS_DISPLAY_GPU;
        desc->present     = true;
        desc->driver_data = &g_i915;
        const char *model = "Intel UHD 610/620 (i915)";
        for (int i = 0; model[i] && i < 63; i++) desc->model[i] = model[i];
        vfs_register_device(DEV_CLASS_DISPLAY_GPU, DEV_CLASS_DISPLAY_GPU, 0, desc);
    }

    /* Route console output to the pixel framebuffer so boot messages remain
     * visible on the physical display once the i915 scanout is active. */
    console_enable_fb(g_i915.fb.virt_addr,
                      g_i915.fb.width, g_i915.fb.height,
                      g_i915.fb.stride, g_i915.fb.bpp);

    kputs("[i915] Display initialised → /zirv/display/gpu0\n");
}

const i915_framebuffer_t *i915_get_fb(void)
{
    return g_i915_found ? &g_i915.fb : NULL;
}
