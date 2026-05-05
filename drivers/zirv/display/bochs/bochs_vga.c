/* drivers/zirv/display/bochs/bochs_vga.c
 * Zirvium Kernel — Bochs/QEMU VGA display driver
 *
 * Initialises the Bochs VBE framebuffer device that QEMU provides for its
 * standard VGA adapter (PCI 0x1234:0x1111) and the dedicated bochs-display
 * device (PCI 0x1234:0x2111).
 *
 * Sequence:
 *  1. Scan PCI for vendor 0x1234, device 0x1111 or 0x2111.
 *  2. Enable the device and map BAR0 (linear framebuffer).
 *  3. Program resolution and BPP via Bochs VBE I/O ports.
 *  4. Clear the framebuffer to black.
 *  5. Register the device as /zirv/display/gpu0.
 */
#include "bochs_vga.h"
#include "drivers/pci/pci.h"
#include "drivers/zirv/device.h"
#include "kernel/console.h"
#include "kernel/mm/vmm.h"
#include "arch/x64/cpu.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Static state ─────────────────────────────────────────────────────────── */
static bochs_vga_fb_t g_fb;

/* ── Bochs VBE register helpers ───────────────────────────────────────────── */
static void vbe_write(uint16_t index, uint16_t value)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA,  value);
}

static uint16_t vbe_read(uint16_t index)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

/* ── Public API ───────────────────────────────────────────────────────────── */
static int bochs_vga_probe(void *hw_desc)
{
    pci_dev_t *pdev = (pci_dev_t *)hw_desc;
    pci_enable_device(pdev);

    /* BAR0 holds the linear framebuffer */
    void *fb_virt = pci_map_bar(pdev, 0);
    if (!fb_virt) return -1;

    /* Verify the Bochs VBE interface is present */
    uint16_t ver = vbe_read(VBE_DISPI_INDEX_ID);
    if ((ver & 0xB0C0u) != 0xB0C0u) return -1;

    /* Disable VBE before reprogramming */
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

    /* Program resolution and colour depth */
    vbe_write(VBE_DISPI_INDEX_XRES, (uint16_t)BOCHS_VGA_WIDTH);
    vbe_write(VBE_DISPI_INDEX_YRES, (uint16_t)BOCHS_VGA_HEIGHT);
    vbe_write(VBE_DISPI_INDEX_BPP,  (uint16_t)BOCHS_VGA_BPP);

    /* Enable with linear framebuffer */
    vbe_write(VBE_DISPI_INDEX_ENABLE,
              VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    /* Fill in the descriptor */
    g_fb.fb_virt = fb_virt;
    g_fb.fb_phys = pdev->bars[0].phys_addr;
    g_fb.width   = BOCHS_VGA_WIDTH;
    g_fb.height  = BOCHS_VGA_HEIGHT;
    g_fb.stride  = BOCHS_VGA_WIDTH * (BOCHS_VGA_BPP / 8u);
    g_fb.bpp     = BOCHS_VGA_BPP;
    g_fb.ready   = true;

    /* Clear the framebuffer to black */
    memset(fb_virt, 0, g_fb.stride * g_fb.height);

    /* Register with /zirv/display/gpu0 */
    zirv_register_device(DEV_CLASS_DISPLAY_GPU, DEV_CLASS_DISPLAY_GPU,
                         "Bochs/QEMU VGA (bochs_vga)", NULL);

    /* Route console output to the pixel framebuffer */
    console_enable_fb(fb_virt,
                      g_fb.width, g_fb.height,
                      g_fb.stride, g_fb.bpp);

    return 0;
}

const zirv_driver_t g_bochs_vga_driver = {
    .name = "bochs_vga",
    .component_tag = "VGA ",
    .type = DRIVER_TYPE_PCI,
    .match.pci = {
        .vendor_id = BOCHS_VGA_VENDOR_ID,
        .device_id = BOCHS_VGA_DEVICE_STD
    },
    .probe = bochs_vga_probe
};

void bochs_vga_init(void)
{
    kputs("[bochs_vga] Scanning PCI for Bochs/QEMU VGA device\n");

    /* Try both the stdvga and the bochs-display PCI IDs */
    pci_dev_t *pdev = pci_find_device(BOCHS_VGA_VENDOR_ID, BOCHS_VGA_DEVICE_STD);
    if (!pdev)
        pdev = pci_find_device(BOCHS_VGA_VENDOR_ID, BOCHS_VGA_DEVICE_BX);

    if (!pdev) {
        kputs("[bochs_vga] Bochs/QEMU VGA not found\n");
        return;
    }

    kputs("[bochs_vga] Bochs/QEMU VGA found\n");
    pci_enable_device(pdev);

    /* BAR0 holds the linear framebuffer */
    void *fb_virt = pci_map_bar(pdev, 0);
    if (!fb_virt) {
        kputs("[bochs_vga] Failed to map BAR0\n");
        return;
    }

    /* Verify the Bochs VBE interface is present */
    uint16_t ver = vbe_read(VBE_DISPI_INDEX_ID);
    if ((ver & 0xB0C0u) != 0xB0C0u) {
        kputs("[bochs_vga] Bochs VBE extensions not detected\n");
        return;
    }

    /* Disable VBE before reprogramming */
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

    /* Program resolution and colour depth */
    vbe_write(VBE_DISPI_INDEX_XRES, (uint16_t)BOCHS_VGA_WIDTH);
    vbe_write(VBE_DISPI_INDEX_YRES, (uint16_t)BOCHS_VGA_HEIGHT);
    vbe_write(VBE_DISPI_INDEX_BPP,  (uint16_t)BOCHS_VGA_BPP);

    /* Enable with linear framebuffer */
    vbe_write(VBE_DISPI_INDEX_ENABLE,
              VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    /* Fill in the descriptor */
    g_fb.fb_virt = fb_virt;
    g_fb.fb_phys = pdev->bars[0].phys_addr;
    g_fb.width   = BOCHS_VGA_WIDTH;
    g_fb.height  = BOCHS_VGA_HEIGHT;
    g_fb.stride  = BOCHS_VGA_WIDTH * (BOCHS_VGA_BPP / 8u);
    g_fb.bpp     = BOCHS_VGA_BPP;
    g_fb.ready   = true;

    /* Clear the framebuffer to black */
    memset(fb_virt, 0, g_fb.stride * g_fb.height);

    /* Register with /zirv/display/gpu0 */
    zirv_register_device(DEV_CLASS_DISPLAY_GPU, DEV_CLASS_DISPLAY_GPU,
                         "Bochs/QEMU VGA (bochs_vga)", NULL);

    /* Route console output to the pixel framebuffer so boot messages remain
     * visible in the QEMU SDL window now that VBE pixel mode is active. */
    console_enable_fb(fb_virt,
                      g_fb.width, g_fb.height,
                      g_fb.stride, g_fb.bpp);

    kputs("[bochs_vga] Framebuffer ready → /zirv/display/gpu0\n");
}

const bochs_vga_fb_t *bochs_vga_get_fb(void)
{
    return g_fb.ready ? &g_fb : NULL;
}
