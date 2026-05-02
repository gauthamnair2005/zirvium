/* kernel/kernel.ld
 * Zirvium Kernel — x86-64 linker script
 *
 * Layout:
 *   Physical load address: 0x100000 (1 MiB — above the BIOS data area)
 *   Virtual link address:  0xFFFFFFFF80100000 (upper half)
 *
 * Sections follow the standard ELF convention and are 4-KiB aligned
 * so page-table permissions can be applied precisely.
 */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

/* Physical load base = 1 MiB */
PHYS_BASE = 0x0000000000100000;
/* Virtual base for the kernel image */
VIRT_BASE = 0xFFFFFFFF80000000;
/* KASLR offset (0 for now) */
KASLR_OFF = 0x0000000000100000;

KERNEL_VIRT = VIRT_BASE + KASLR_OFF;

SECTIONS
{
    /* ── Boot / multiboot2 header — must be within first 32 KiB of image ── */
    . = PHYS_BASE;

    .multiboot2 : AT(PHYS_BASE) {
        KEEP(*(.multiboot2))
    }

    /* ── Kernel text ──────────────────────────────────────────────────────── */
    . = KERNEL_VIRT;

    _kernel_start = .;

    .text ALIGN(4096) : AT(ADDR(.text) - VIRT_BASE + PHYS_BASE) {
        *(.text .text.*)
    }

    /* ── Read-only data ───────────────────────────────────────────────────── */
    .rodata ALIGN(4096) : AT(ADDR(.rodata) - VIRT_BASE + PHYS_BASE) {
        *(.rodata .rodata.*)
    }

    /* ── Initialised data ─────────────────────────────────────────────────── */
    .data ALIGN(4096) : AT(ADDR(.data) - VIRT_BASE + PHYS_BASE) {
        *(.data .data.*)
    }

    /* ── BSS (zero-initialised) ───────────────────────────────────────────── */
    .bss ALIGN(4096) : AT(ADDR(.bss) - VIRT_BASE + PHYS_BASE) {
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
