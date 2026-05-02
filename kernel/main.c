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
#include "fs/mosix.h"
#include "drivers/serial/serial.h"
#include "drivers/pci/pci.h"
#include "drivers/zirv/device.h"
#include "drivers/zirv/nvme.h"
#include "drivers/zirv/usb_storage.h"
#include "drivers/zirv/input/ps2/i8042.h"
#include "drivers/zirv/input/ps2/keyboard.h"
#include "drivers/zirv/input/ps2/synaptics.h"
#include "drivers/zirv/wifi/rtl8723de/rtl8723de.h"
#include "drivers/zirv/bluetooth/btrtl.h"
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
    serial_puts(SERIAL_COM1, "\n[ZIRVIUM PANIC] ");
    serial_puts(SERIAL_COM1, msg);
    serial_puts(SERIAL_COM1, "\n");
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

/* ── Simple decimal formatter for serial output ──────────────────────────── */
static void serial_putdec(uint16_t port, uint64_t val)
{
    char buf[21];
    int i = 20;
    buf[i] = '\0';
    if (val == 0) { serial_putc(port, '0'); return; }
    while (val && i > 0) {
        buf[--i] = (char)('0' + val % 10);
        val /= 10;
    }
    serial_puts(port, buf + i);
}

/* ── kernel_main ──────────────────────────────────────────────────────────── */
void kernel_main(uint32_t multiboot2_magic, uint32_t mb2_info_phys)
{
    /* ── Step 1: Serial console (very early, no memory manager needed) ─── */
    serial_init(SERIAL_COM1);
    serial_puts(SERIAL_COM1, "Zirvium Kernel starting...\n");

    /* ── Step 2: Validate Multiboot2 magic ────────────────────────────── */
    if (multiboot2_magic != MB2_BOOTLOADER_MAGIC)
        kpanic("Not loaded by a Multiboot2-compliant bootloader");

    /* ── Step 3: Set up a proper GDT + TSS ───────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] GDT\n");
    gdt_init();
    gdt_load_tss();

    /* ── Step 4: Set up IDT (exception + IRQ handling) ───────────────── */
    serial_puts(SERIAL_COM1, "[init] IDT\n");
    idt_init();

    /* ── Step 5: Parse Multiboot2 memory map ─────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] PMM\n");
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

    serial_puts(SERIAL_COM1, "[pmm] Total pages: ");
    serial_putdec(SERIAL_COM1, pmm_total_pages());
    serial_puts(SERIAL_COM1, "  Free: ");
    serial_putdec(SERIAL_COM1, pmm_free_page_count());
    serial_puts(SERIAL_COM1, "\n");

    /* ── Step 6: Virtual memory manager ─────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] VMM\n");
    vmm_init();

    /* ── Step 7: MOSIX VFS root namespace ─────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] MOSIX VFS\n");
    vfs_init();

    /* ── Step 8: Device registry ─────────────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] /zirv device registry\n");
    zirv_dev_init();

    /* ── Step 9: PCI bus enumeration ─────────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] PCI bus scan\n");
    pci_init();

    /* ── Step 10: Storage drivers ─────────────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] SATA/PATA\n");
    sata_init();

    serial_puts(SERIAL_COM1, "[init] NVMe\n");
    nvme_init();

    serial_puts(SERIAL_COM1, "[init] USB storage\n");
    usb_storage_init();

    /* ── Step 11: IRQ subsystem (8259A PIC) ───────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] IRQ / PIC\n");
    irq_init();

    /* ── Step 12: Input devices ───────────────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] PS/2 controller (i8042)\n");
    i8042_init();

    serial_puts(SERIAL_COM1, "[init] PS/2 keyboard\n");
    keyboard_init();

    serial_puts(SERIAL_COM1, "[init] Synaptics touchpad\n");
    synaptics_init();

    /* ── Step 13: WiFi — RTL8723DE ────────────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] RTL8723DE WiFi\n");
    rtl8723de_init();

    /* ── Step 14: Bluetooth — RTL8723DE ───────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] RTL8723DE Bluetooth\n");
    btrtl_init(0);   /* 0 = auto-detect UART port */

    /* ── Step 15: Intel i915 display ─────────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] Intel i915 display (UHD 610 → Arc)\n");
    i915_init();

    /* ── Step 16: Intel HDA audio ────────────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] Intel HDA audio\n");
    hda_init();

    /* ── Step 17: Enable interrupts ───────────────────────────────────── */
    serial_puts(SERIAL_COM1, "[init] Enabling interrupts\n");
    __asm__ volatile("sti");

    /* ── Boot complete ────────────────────────────────────────────────── */
    serial_puts(SERIAL_COM1,
        "\n"
        "============================================\n"
        " Zirvium Kernel boot complete\n"
        " MOSIX filesystem hierarchy active:\n"
        "   /bin  /lib  /user  /boot\n"
        "   /config  /zirv  /mounts  /tmp\n"
        " /zirv device namespace populated:\n"
        "   /zirv/sata/*  /zirv/nvme/*\n"
        "   /zirv/net/wlan0  /zirv/net/bt0\n"
        "   /zirv/display/gpu0\n"
        "   /zirv/audio/output0\n"
        "   /zirv/input/keyboard0\n"
        "   /zirv/input/touchpad0\n"
        "============================================\n"
    );

    /* Idle loop — scheduler / user-space init goes here */
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
        serial_puts(SERIAL_COM1, "[EXCEPTION] #");
        serial_putdec(SERIAL_COM1, s->int_no);
        serial_puts(SERIAL_COM1, " err=");
        serial_putdec(SERIAL_COM1, s->err_code);
        serial_puts(SERIAL_COM1, "\n");
        __asm__ volatile("cli; hlt");
    }
}
