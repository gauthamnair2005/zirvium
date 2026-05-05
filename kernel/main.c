/* kernel/main.c
 * Zirvium Kernel — Core initialisation and main entry point.
 */
#include "kernel/console.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/irq/irq.h"
#include "kernel/proc/process.h"
#include "fs/mosix.h"
#include "drivers/pci/pci.h"
#include "drivers/vga/vga.h"
#include "drivers/serial/serial.h"
#include "drivers/zirv/driver.h"
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
/* isr_dispatch is called by isr_common_stub (arch/x64/isr_stubs.asm)
 * for all CPU exceptions and hardware interrupts. */
void isr_dispatch(cpu_state_t *state)
{
    if (state->int_no < 32) {
        /* Exception */
        kprintf("\n[ CPU EXCEPTION %d ] error_code=0x%lx rip=0x%lx\n",
                (int)state->int_no, state->err_code, state->rip);
        kpanic("Unhandled CPU exception");
    } else {
        /* Hardware Interrupt */
        irq_dispatch((int)state->int_no);
    }
}

/* ── kernel_main ──────────────────────────────────────────────────────────── */
void kernel_main(uint32_t magic, uint32_t info_phys)
{
    /* ── Step 1: Early console ───────────────────────────────────────── */
    serial_init(SERIAL_COM1);
    vga_init();
    console_init();
    console_enable_vga();

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

    /* ── Step 3: Memory Management ───────────────────────────────────── */
    /* Find the memory map tag in the Multiboot2 info structure */
    uint32_t total_info_size = *(uint32_t *)(uintptr_t)info_phys;
    uintptr_t tag_ptr = (uintptr_t)info_phys + 8;
    bool mmap_found = false;

    while (tag_ptr < (uintptr_t)info_phys + total_info_size) {
        mb2_tag_t *tag = (mb2_tag_t *)tag_ptr;
        if (tag->type == 0) break; /* End tag */

        if (tag->type == MB2_TAG_MMAP) {
            mb2_mmap_tag_t *mt = (mb2_mmap_tag_t *)tag;
            pmm_init((uint64_t)(tag_ptr + 16), mt->size - 16, mt->entry_size);
            mmap_found = true;
            break;
        }
        tag_ptr = (tag_ptr + tag->size + 7) & ~7; /* 8-byte aligned */
    }

    if (!mmap_found)
        kpanic("No Multiboot2 memory map provided");

    klog(LOG_INFO, "PMM ", "Total pages: %u, Free: %u", 
         (uint32_t)pmm_total_pages(), (uint32_t)pmm_free_page_count());

    vmm_init();
    klog(LOG_OK, "VMM ", "Virtual memory active (Higher-Half)");

    /* ── Step 4: Core Subsystems ─────────────────────────────────────── */
    vfs_init();
    proc_init();

    /* ── Step 5: Hardware Discovery ──────────────────────────────────── */
    pci_init();
    klog(LOG_INFO, "PCI ", "Discovered %d devices", (int)pci_device_count());

    /* ── Step 6: Dynamic Driver Probing ──────────────────────────────── */
    /* Register available drivers */
    extern const zirv_driver_t g_bochs_vga_driver;
    extern const zirv_driver_t g_vmware_svga_driver;
    extern const zirv_driver_t g_tpm2_driver;
    extern const zirv_driver_t g_intel_e1000_driver;
    extern const zirv_driver_t g_virtio_net_driver;
    extern const zirv_driver_t g_virtio_blk_driver;
    extern const zirv_driver_t g_rtl8139_driver;

    driver_register(&g_bochs_vga_driver);
    driver_register(&g_vmware_svga_driver);
    driver_register(&g_tpm2_driver);
    driver_register(&g_intel_e1000_driver);
    driver_register(&g_virtio_net_driver);
    driver_register(&g_virtio_blk_driver);
    driver_register(&g_rtl8139_driver);

    klog(LOG_INFO, "DRV ", "Probing hardware for matching drivers...");
    driver_probe_all();

    /* ── Step 7: Launch Shell ────────────────────────────────────────── */
    kputs("\n");
    console_set_color(CONSOLE_COLOR_GREEN);
    kputs("  [ SUCCESS ] Loaded Zirvium Kernel successfully.\n");
    console_reset_color();
    kprintf("Zirvium Kernel 0.4\n");
    kprintf("Gautham Nair\n");
    
    /* TODO: launch zirvshell */
    for (;;) {
        __asm__ volatile("hlt");
    }
}
