/* kernel/main.c
 * Zirvium Kernel — kernel_main(): first C function called after boot
 *
 * Called from arch/x64/boot.asm once the CPU is in 64-bit long mode and
 * the kernel stack has been set up.
 *
 *  @multiboot2_magic  — must equal 0x36D76289 to confirm a valid MB2 loader
 *  @mb2_info_phys     — physical address of the Multiboot2 information structure
 */
#include "arch/x64/gdt.h"
#include "arch/x64/idt.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/irq/irq.h"
#include "kernel/proc/process.h"
#include "kernel/syscall/syscall.h"
#include "kernel/console.h"
#include "fs/mosix.h"
#include "drivers/serial/serial.h"
#include "drivers/vga/vga.h"
#include "drivers/pci/pci.h"
#include "drivers/zirv/device.h"
#include "drivers/zirv/nvme.h"
#include "drivers/zirv/usb_storage.h"
#include "drivers/zirv/input/ps2/i8042.h"
#include "drivers/zirv/input/ps2/keyboard.h"
#include "drivers/zirv/input/ps2/synaptics.h"
#include "drivers/zirv/wifi/rtl8723de/rtl8723de.h"
#include "drivers/zirv/bluetooth/btrtl.h"
#include "drivers/zirv/display/bochs/bochs_vga.h"
#include "drivers/zirv/display/i915/i915.h"
#include "drivers/zirv/audio/hda/hda.h"

#include <stdint.h>
#include <stddef.h>

/* Forward declaration for SATA init (drivers/zirv/sata.c) */
void sata_init(void);

/* Forward declaration for ISR dispatch (arch/x64/) */
void isr_dispatch_handler(void *state);

/* ── Multiboot2 constants ─────────────────────────────────────────────────── */
#define MB2_BOOTLOADER_MAGIC  0x36D76289U

/* Multiboot2 information structure tags */
#define MB2_TAG_END           0
#define MB2_TAG_CMDLINE       1
#define MB2_TAG_MODULES       3
#define MB2_TAG_BASIC_MEMINFO 4
#define MB2_TAG_MMAP          6
#define MB2_TAG_FRAMEBUFFER   8

typedef struct __attribute__((packed)) {
    uint32_t total_size;
    uint32_t reserved;
} mb2_info_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
} mb2_tag_t;

typedef struct __attribute__((packed)) {
    uint32_t type;        /* 6 */
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    /* mmap entries follow */
} mb2_mmap_tag_t;

/* ── Panic ────────────────────────────────────────────────────────────────── */
static void kpanic(const char *msg)
{
    kputs("\n[ZIRVIUM PANIC] ");
    kputs(msg);
    kputs("\n");
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}


/* ── kernel_main ──────────────────────────────────────────────────────────── */
void kernel_main(uint32_t multiboot2_magic, uint32_t mb2_info_phys)
{
    /* ── Step 1: Serial console + VGA text console (very early) ──────── */
    serial_init(SERIAL_COM1);
    vga_init();
    console_init();
    console_enable_vga();
    kputs("Loading Zirvium Kernel\n");

    /* ── Step 2: Validate Multiboot2 magic ────────────────────────────── */
    if (multiboot2_magic != MB2_BOOTLOADER_MAGIC)
        kpanic("Not loaded by a Multiboot2-compliant bootloader");

    /* ── Step 3: Set up a proper GDT + TSS ───────────────────────────── */
    gdt_init();
    gdt_load_tss();
    klog(LOG_OK, "GDT", "Global Descriptor Table initialised");

    /* ── Step 4: Set up IDT (exception + IRQ handling) ───────────────── */
    idt_init();
    klog(LOG_OK, "IDT", "Interrupt Descriptor Table initialised");

    /* ── Step 5: Parse Multiboot2 memory map ─────────────────────────── */
    const mb2_info_t *info = (const mb2_info_t *)(uintptr_t)mb2_info_phys;
    const mb2_mmap_tag_t *mmap_tag = NULL;

    /* Walk tags */
    const uint8_t *p = (const uint8_t *)(info + 1);
    const uint8_t *end = (const uint8_t *)info + info->total_size;
    while (p < end) {
        const mb2_tag_t *tag = (const mb2_tag_t *)p;
        if (tag->type == MB2_TAG_END) break;
        if (tag->type == MB2_TAG_MMAP)
            mmap_tag = (const mb2_mmap_tag_t *)tag;
        /* Tags are 8-byte aligned */
        p += (tag->size + 7) & ~7U;
    }

    if (!mmap_tag)
        kpanic("No Multiboot2 memory map found");

    uint64_t mmap_entries_addr = (uint64_t)(uintptr_t)(mmap_tag + 1);
    uint32_t mmap_len = mmap_tag->size
                      - (uint32_t)sizeof(mb2_mmap_tag_t);
    pmm_init(mmap_entries_addr, mmap_len, mmap_tag->entry_size);

    klog(LOG_INFO, "PMM", "Total pages: %u, Free: %u", 
         (uint32_t)pmm_total_pages(), (uint32_t)pmm_free_page_count());

    /* ── Step 6: Virtual memory manager ─────────────────────────────── */
    vmm_init();
    klog(LOG_OK, "VMM", "Kernel page tables loaded (1 TiB direct-map ready)");

    /* ── Step 7: MOSIX VFS root namespace ─────────────────────────────── */
    vfs_init();
    klog(LOG_OK, "VFS", "MOSIX root namespace mounted");

    /* ── Step 8: Process subsystem ────────────────────────────────────── */
    proc_init();
    klog(LOG_OK, "PROC", "Process scheduler initialised");

    /* ── Step 9: Syscall interface (SYSCALL / SYSRET) ──────────────────── */
    syscall_init();
    klog(LOG_OK, "SYSC", "Syscall interface active");

    /* ── Step 10: Device registry ─────────────────────────────────────── */
    zirv_dev_init();
    klog(LOG_OK, "DEV", "/zirv device registry ready");

    /* ── Step 11: PCI bus enumeration ─────────────────────────────────── */
    pci_init();
    klog(LOG_OK, "PCI", "Bus enumeration complete");

    /* ── Step 12: Bochs/QEMU VGA framebuffer (requires PCI + VMM) ──────── */
    bochs_vga_init();
    klog(LOG_OK, "VGA", "Bochs/QEMU display driver active");

    /* ── Step 13: Storage drivers ─────────────────────────────────────── */
    sata_init();
    klog(LOG_OK, "SATA", "SATA/PATA controllers probed");

    nvme_init();
    klog(LOG_OK, "NVME", "NVMe storage volumes active");

    usb_storage_init();
    klog(LOG_OK, "USB", "USB mass storage support ready");

    /* ── Step 14: IRQ subsystem (8259A PIC) ───────────────────────────── */
    irq_init();
    klog(LOG_OK, "IRQ", "8259A PIC / IRQ routing enabled");

    /* ── Step 15: Input devices ───────────────────────────────────────── */
    i8042_init();
    klog(LOG_OK, "PS2", "i8042 controller initialised");

    keyboard_init();
    klog(LOG_OK, "KBD", "PS/2 keyboard active");

    synaptics_init();
    klog(LOG_WARN, "SYN", "Synaptics touchpad not found");

    /* ── Step 16: WiFi — RTL8723DE ────────────────────────────────────── */
    rtl8723de_init();
    klog(LOG_WARN, "WIFI", "RTL8723DE hardware missing");

    /* ── Step 17: Bluetooth — RTL8723DE ───────────────────────────────── */
    btrtl_init(0);
    klog(LOG_WARN, "BT", "Bluetooth stack starting (no device)");

    /* ── Step 18: Intel i915 display ─────────────────────────────────── */
    i915_init();
    klog(LOG_OK, "I915", "Intel i915 detector finished");

    /* ── Step 19: Intel HDA audio ────────────────────────────────────── */
    hda_init();
    klog(LOG_WARN, "HDA", "No HDA codecs found");

    /* ── Step 20: Enable interrupts ───────────────────────────────────── */
    __asm__ volatile("sti");
    klog(LOG_OK, "INIT", "Hardware interrupts enabled");

	kputs("[info] Color Codes\n");
	kprint_ok();
	kputs("			This means the component have been loaded without any issue\n");
	kprint_warn();
	kputs("			This means the component is either absent or not loaded properly, however is not a fatal issue and won't hang or crash system\n");

    /* ── Boot complete ────────────────────────────────────────────────── */
	kputs("-----------------------------LOADED----------------------------\n");
    kputs("Zirvium Kernel 0.3.1\nGautham Nair\n");

    /* Idle loop — scheduler / usr-space init goes here */
    for (;;) __asm__ volatile("hlt");
}

/* ── ISR dispatch (called from isr_stubs.asm) ─────────────────────────────── */
void isr_dispatch(void *state)
{
    /* The cpu_state_t is laid out so int_no is at a known offset.
     * Vectors 32–255 are hardware IRQs — route to the IRQ subsystem. */
    typedef struct { uint64_t r15,r14,r13,r12,r11,r10,r9,r8;
                     uint64_t rdi,rsi,rbp,rbx,rdx,rcx,rax;
                     uint64_t int_no, err_code;
                     uint64_t rip, cs, rflags, rsp, ss; } cpu_state_t;
    cpu_state_t *s = (cpu_state_t *)state;

    if (s->int_no >= 32) {
        irq_dispatch((int)s->int_no);
    } else {
        /* CPU exception — log and halt for now */
        klog(LOG_FAIL, "CPU", "Exception #%u (error code: %u)", 
             (uint32_t)s->int_no, (uint32_t)s->err_code);
        __asm__ volatile("cli; hlt");
    }
}
