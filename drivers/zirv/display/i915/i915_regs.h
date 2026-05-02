/* drivers/zirv/display/i915/i915_regs.h
 * Zirvium Kernel — Intel i915 register definitions
 *
 * Covers Gen 9.5 (Kaby Lake / Kaby Lake-R) and Gen 9.5+ (Coffee Lake-U)
 * which includes Intel UHD 610 (0x5916/0x3EA5) and UHD 620 (0x5917/0x3EA5).
 *
 * Register definitions are derived from the Intel Graphics PRMs (publicly
 * available at 01.org / intel.github.io) and the Linux i915 driver
 * (MIT/GPL-2.0, Copyright Intel Corporation).
 */
#ifndef ZIRVIUM_DRIVERS_DISPLAY_I915_REGS_H
#define ZIRVIUM_DRIVERS_DISPLAY_I915_REGS_H

/* ── MMIO register helper macro ──────────────────────────────────────────── */
#define _MMIO(r)  (r)

/* ── Power management ────────────────────────────────────────────────────── */
#define HSW_PWR_WELL_CTL2         _MMIO(0x45404)
#define  HSW_PWR_WELL_CTL_REQ(n)  (1 << (((n) * 2) + 1))
#define  HSW_PWR_WELL_CTL_STATE(n)(1 << ((n) * 2))

#define GEN9_DCPR_ENABLE          _MMIO(0x46460)
#define  MASK_WAKEMEM             (1 << 13)

/* ── PCI device IDs (Gen9.5 / Kaby Lake / Coffee Lake-U) ─────────────────── */
#define PCI_VENDOR_INTEL       0x8086
/* Kaby Lake GT1 — UHD 610 */
#define INTEL_KBL_GT1_ULT      0x5902
#define INTEL_KBL_GT1_ULX      0x590E
/* Kaby Lake-R GT2 — UHD 620 */
#define INTEL_KBLR_GT2         0x5917
/* Coffee Lake-U GT2 — UHD 620 */
#define INTEL_CFL_GT2_ULT      0x3EA5
/* Coffee Lake-U GT1 — UHD 610 */
#define INTEL_CFL_GT1_ULT      0x3EA0

/* ── GT/display generation ────────────────────────────────────────────────── */
#define GEN9                   9

/* ── GTT (Graphics Translation Table) ───────────────────────────────────── */
#define GEN6_PPGTT_PD_ENTRIES  512
#define GEN8_PPGTT_PDL         4

/* Aperture BAR (BAR2 in lspci) */
#define I915_GTT_APERTURE_SIZE (512UL * 1024 * 1024)  /* 512 MiB */

/* ── Interrupt registers ─────────────────────────────────────────────────── */
#define GEN8_MASTER_IRQ           _MMIO(0x44200)
#define  GEN8_MASTER_IRQ_CONTROL  (1u << 31)
#define GEN8_GT_ISR(which)        _MMIO(0x44300 + ((which) * 0x10))
#define GEN8_GT_IMR(which)        _MMIO(0x44304 + ((which) * 0x10))
#define GEN8_GT_IIR(which)        _MMIO(0x44308 + ((which) * 0x10))
#define GEN8_GT_IER(which)        _MMIO(0x4430C + ((which) * 0x10))

/* ── Display engine registers ────────────────────────────────────────────── */
/* Pipe registers (pipe A = 0, B = 1, C = 2) */
#define PIPE_A 0
#define PIPE_B 1
#define PIPE_C 2

#define PIPECONF(pipe)            _MMIO(0x70008 + (pipe) * 0x1000)
#define  PIPECONF_ENABLE          (1u << 31)
#define  PIPECONF_DOUBLE_WIDE     (1u << 30)
#define  PIPECONF_BPC_8           (0u << 5)
#define  PIPECONF_BPC_10          (1u << 5)

/* Display plane (primary plane) */
#define DSPCNTR(plane)            _MMIO(0x70180 + (plane) * 0x1000)
#define  DISPLAY_PLANE_ENABLE     (1u << 31)
#define  DISPPLANE_RGBX888        (0x6u << 26)

#define DSPSTRIDE(plane)          _MMIO(0x70188 + (plane) * 0x1000)
#define DSPSURF(plane)            _MMIO(0x7019C + (plane) * 0x1000)
#define DSPLINOFF(plane)          _MMIO(0x70184 + (plane) * 0x1000)
#define DSPTILEOFF(plane)         _MMIO(0x701A4 + (plane) * 0x1000)

/* Pipe timing */
#define HTOTAL(pipe)              _MMIO(0x60000 + (pipe) * 0x1000)
#define HBLANK(pipe)              _MMIO(0x60004 + (pipe) * 0x1000)
#define HSYNC(pipe)               _MMIO(0x60008 + (pipe) * 0x1000)
#define VTOTAL(pipe)              _MMIO(0x6000C + (pipe) * 0x1000)
#define VBLANK(pipe)              _MMIO(0x60010 + (pipe) * 0x1000)
#define VSYNC(pipe)               _MMIO(0x60014 + (pipe) * 0x1000)

/* DDI (Digital Display Interface) — replaces legacy LVDS/HDMI/DP encoders */
#define DDI_BUF_CTL(port)         _MMIO(0x64000 + (port) * 0x100)
#define  DDI_BUF_CTL_ENABLE       (1u << 31)

#define TRANS_DDI_FUNC_CTL(trans) _MMIO(0x60400 + (trans) * 0x1000)
#define  TRANS_DDI_FUNC_ENABLE    (1u << 31)
#define  TRANS_DDI_PORT_SHIFT     28
#define  TRANS_DDI_MODE_SELECT_DP_SST (0x2u << 24)

/* eDP panel power sequencing */
#define PP_STATUS                 _MMIO(0xC7200)
#define  PP_ON                    (1u << 31)
#define PP_CONTROL                _MMIO(0xC7204)
#define  PANEL_POWER_ON           (1u << 0)
#define  PANEL_POWER_RESET        (1u << 1)

/* Backlight */
#define BLC_PWM_CPU_CTL2          _MMIO(0x48250)
#define BLC_PWM_CPU_CTL           _MMIO(0x48254)
#define  BACKLIGHT_DUTY_CYCLE_SHIFT 16

/* ── GMBUS (I2C for DDC / EDID) ───────────────────────────────────────────── */
#define GMBUS0                    _MMIO(0xC5100)
#define GMBUS1                    _MMIO(0xC5104)
#define  GMBUS_SW_CLR_INT         (1u << 31)
#define  GMBUS_SW_RDY             (1u << 30)
#define  GMBUS_CYCLE_INDEX        (1u << 25)
#define  GMBUS_CYCLE_WAIT         (3u << 25)
#define  GMBUS_BYTE_CNT_SHIFT     16
#define  GMBUS_SLAVE_ADDR_SHIFT   1
#define GMBUS2                    _MMIO(0xC5108)
#define  GMBUS_ACTIVE             (1u << 15)
#define  GMBUS_HW_RDY             (1u << 11)
#define  GMBUS_SATOER             (1u << 10)
#define  GMBUS_INTR_STAT          (1u << 9)
#define GMBUS3                    _MMIO(0xC510C)  /* Data register */
#define GMBUS4                    _MMIO(0xC5110)
#define GMBUS5                    _MMIO(0xC5120)

#endif /* ZIRVIUM_DRIVERS_DISPLAY_I915_REGS_H */
