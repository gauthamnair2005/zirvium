/* kernel/kernel.ld
 * Zirvium Kernel — x86-64 linker script
 *
 * Layout:
 *   Boot code/data physical load address : 0x100000 (1 MiB)
 *   Kernel physical load address         : 0x200000 (2 MiB — 2 MiB aligned for
 *                                          huge-page mapping from the boot stub)
 *   Kernel virtual link address          : 0xFFFFFFFF80000000 (upper half)
 *
 * The boot stub (_start in .boot_text) runs at its physical address before
 * paging is enabled, so it has VMA = LMA.  Once the boot stub enables paging
 * it performs an absolute call to kernel_main which lives at its high VMA.
 */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

/* Physical load address for the boot stub and embedded multiboot2 header */
PHYS_BOOT   = 0x0000000000100000;
/* Physical load address for the kernel proper (must be 2 MiB aligned) */
PHYS_KERNEL = 0x0000000000200000;
/* Virtual base for the higher-half kernel */
VIRT_BASE   = 0xFFFFFFFF80000000;

SECTIONS
{
    /* ── Boot sections (VMA = LMA = physical) ────────────────────────────── */
    . = PHYS_BOOT;

    /* multiboot2 header must be in the first 32 KiB of the binary image */
    .multiboot2 : {
        KEEP(*(.multiboot2))
    }

    /* 32-bit entry code + 64-bit bridge (long_mode_entry) */
    .boot_text : {
        *(.boot_text)
    }

    /* GDT and other boot-time initialised data */
    .boot_data : {
        *(.boot_data)
    }

    /* Page tables and boot stack — zero-initialised, not stored in file */
    .boot_bss (NOLOAD) : {
        *(.boot_bss)
    }

    /* ── Kernel proper (VMA = VIRT_BASE, LMA = PHYS_KERNEL + offset) ────── */
    . = VIRT_BASE;

    _kernel_start = .;

    .text ALIGN(4096) : AT(ADDR(.text) - VIRT_BASE + PHYS_KERNEL) {
        *(.text .text.*)
    }

    /* ── Read-only data ───────────────────────────────────────────────────── */
    .rodata ALIGN(4096) : AT(ADDR(.rodata) - VIRT_BASE + PHYS_KERNEL) {
        *(.rodata .rodata.*)
    }

    /* ── Initialised data ─────────────────────────────────────────────────── */
    .data ALIGN(4096) : AT(ADDR(.data) - VIRT_BASE + PHYS_KERNEL) {
        *(.data .data.*)
    }

    /* ── BSS (zero-initialised by kernel itself after mapping is live) ───── */
    .bss ALIGN(4096) (NOLOAD) : AT(ADDR(.bss) - VIRT_BASE + PHYS_KERNEL) {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    }

    _kernel_end = .;

    /* ── Discard unwanted sections ────────────────────────────────────────── */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
