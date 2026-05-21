/* kernel/main.c
 * Zirvium Kernel — Core initialisation and main entry point.
 */
#include "kernel/console.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/irq/irq.h"
#include "kernel/proc/process.h"
#include "kernel/loader/elf.h"
#include "kernel/loader/embedded.h"
#include "fs/mosix.h"
#include "drivers/pci/pci.h"
#include "drivers/vga/vga.h"
#include "drivers/serial/serial.h"
#include "drivers/zirv/input/ps2/i8042.h"
#include "drivers/zirv/input/ps2/keyboard.h"
#include "drivers/zirv/driver.h"
#include "drivers/gpu/enveediya/enveediya.h"
#include "drivers/gpu/arc/arc.h"
#include "drivers/gpu/radeon/radeon.h"
#include "drivers/usb/xhci/xhci.h"
#include "drivers/thunderbolt/tb.h"
#include "drivers/net/wifi7/wifi7.h"
#include "drivers/net/rtl8139/rtl8139.h"
#include "drivers/zirv/displayjet/displayjet.h"
#include "arch/x64/gdt.h"
#include "arch/x64/idt.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define MULTIBOOT2_MAGIC_OK 0x36D76289

/* ── Multiboot2 info tag types ────────────────────────────────────────────── */
#define MB2_TAG_MMAP    6

typedef struct {
    uint32_t type;
    uint32_t size;
} mb2_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} mb2_mmap_tag_t;

/* ── Global hostname ──────────────────────────────────────────────────────── */
char g_hostname[64] = "zirvium";

/* ── Embedded Binaries ────────────────────────────────────────────────────── */
extern char zirvinit_bin_start[];

/* ── Panic ────────────────────────────────────────────────────────────────── */
static void kpanic(const char *msg)
{
    console_set_color(CONSOLE_COLOR_RED);
    kprintf("\n[ KERNEL PANIC ] %s\n", msg);
    kprintf("System halted.\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

/* ── ISR / IRQ Dispatch ─────────────────────────────────────────────────── */
void isr_dispatch(cpu_state_t *state)
{
    if (state->int_no < 32) {
        kprintf("\n[ CPU EXCEPTION %d ] error_code=0x%lx rip=0x%lx\n",
                (int)state->int_no, state->err_code, state->rip);
        kpanic("Unhandled CPU exception");
    } else {
        irq_dispatch((int)state->int_no);
    }
}

/* ── kernel_main ──────────────────────────────────────────────────────────── */
void kernel_main(uint32_t magic, uint32_t info_phys)
{
    /* ── Step 1: Early serial (VGA comes after VMM) ──────────────────── */
    serial_init(SERIAL_COM1);

    /* ── Beautiful Splash Header ─────────────────────────────────────── */
    console_set_color(CONSOLE_COLOR_GREEN);
    kputs("\n");
    kputs("  Z I R V I U M    K E R N E L\n");
    kputs("  ────────────────────────────\n");
    console_reset_color();
    kprintf("  Developer: Gautham Nair\n");
    kputs("  Status:    Loading Zirvium Kernel...\n\n");

    /* ── Step 2: CPU Foundations ─────────────────────────────────────── */
    gdt_init();
    idt_init();
    irq_init();

    if (magic != MULTIBOOT2_MAGIC_OK)
        kpanic("Invalid Multiboot2 magic number");

    /* Load TSS so interrupts from user mode have a valid kernel stack */
    gdt_load_tss();

    /* ── Step 3: Memory Management ───────────────────────────────────── */
    uint32_t total_info_size = *(uint32_t *)(uintptr_t)info_phys;
    uintptr_t tag_ptr = (uintptr_t)info_phys + 8;
    bool mmap_found = false;

    while (tag_ptr < (uintptr_t)info_phys + total_info_size) {
        mb2_tag_t *tag = (mb2_tag_t *)tag_ptr;
        if (tag->type == 0) break;
        if (tag->type == MB2_TAG_MMAP) {
            mb2_mmap_tag_t *mt = (mb2_mmap_tag_t *)tag;
            pmm_init((uint64_t)(tag_ptr + 16), mt->size - 16, mt->entry_size);
            mmap_found = true;
            break;
        }
        tag_ptr = (tag_ptr + tag->size + 7) & ~7;
    }

    if (!mmap_found) kpanic("No Multiboot2 memory map provided");

    vmm_init();

    /* VGA text console — must come after vmm_init because the driver uses
     * the direct physical map (PHYS_MAP_BASE) instead of the identity map,
     * so the VGA buffer at 0xB8000 remains accessible from every address
     * space (kernel and user).
     */
    vga_init();
    console_init();
    console_enable_vga();

    klog(LOG_OK, "VMM ", "Virtual memory active (Higher-Half)");

    /* ── Step 4: Core Subsystems ─────────────────────────────────────── */
    vfs_init();
    proc_init();
    embedded_init();
    
    extern void syscall_init(void);
    syscall_init();

    extern void time_init(void);
    time_init();

    /* ── Step 5: Hardware Discovery ──────────────────────────────────── */
    pci_init();
    
    /* ── Step 6: Dynamic Driver Probing ──────────────────────────────── */
    /* bochs_vga is superseded by displayjet (DisplayJet w/ MAEM) */
    extern const zirv_driver_t g_vmware_svga_driver;
    extern const zirv_driver_t g_tpm2_driver;
    extern const zirv_driver_t g_intel_e1000_driver;

    driver_register(&g_vmware_svga_driver);
    driver_register(&g_tpm2_driver);
    driver_register(&g_intel_e1000_driver);

    driver_probe_all();

    /* Ported Linux VirtIO drivers — use Linux-compat PCI probe */
    extern void virtio_init(void);
    virtio_init();

    /* ── DisplayJet (user-space display framework, supersedes bochs_vga) ─ */
    displayjet_init();

    /* ── Linux-compat drivers (conditional on PCI detection) ──────────── */
    enveediya_init();
    arc_gpu_init();
    radeon_gpu_init();
    xhci_init();
    thunderbolt_init();
    wifi7_init();
    rtl8139_init();

    /* ── Step 6b: Legacy PS/2 Keyboard ───────────────────────────────── */
    if (i8042_init())
        keyboard_init();

    /* ── Step 7: Launch MOSIX Init ───────────────────────────────────── */
    kputs("\n");
    console_set_color(CONSOLE_COLOR_GREEN);
    kputs("  [ SUCCESS ] Loaded Zirvium Kernel successfully.\n");
    console_reset_color();
    kprintf("  System ready. Launching zirvinit...\n\n");

    process_t *init = proc_create(0);
    if (!init) kpanic("Failed to create PID 1");

    if (!elf_load_process(init, zirvinit_bin_start))
        kpanic("Failed to load zirvinit.elf");

    klog(LOG_OK, "INIT", "zirvinit loaded at entry %p", (void *)init->user_rip);

    extern void proc_init_stdio(process_t *proc);
    proc_init_stdio(init);

    proc_enter_usermode(init);
}
