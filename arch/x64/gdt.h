/* arch/x64/gdt.h
 * Zirvium Kernel — Global Descriptor Table helpers (x86-64)
 *
 * The boot GDT (in boot.asm) has three entries:
 *   0x00 – null descriptor
 *   0x08 – 64-bit kernel code  (DPL 0)
 *   0x10 – 64-bit kernel data  (DPL 0)
 *
 * A fuller GDT with user-space and TSS entries is installed by gdt_init().
 */
#ifndef ZIRVIUM_ARCH_X64_GDT_H
#define ZIRVIUM_ARCH_X64_GDT_H

#include <stdint.h>

/* Segment selector constants */
#define GDT_NULL          0x00
#define GDT_KERNEL_CODE   0x08
#define GDT_KERNEL_DATA   0x10
/* User segments: data must precede code so SYSRETQ can derive both selectors
 * from a single STAR[63:48] base (SS = base+8, CS = base+16). */
#define GDT_USER_DATA     0x18
#define GDT_USER_CODE     0x20
#define GDT_TSS_LOW       0x28   /* 16-byte system descriptor occupies two slots */

/* Access byte flags */
#define GDT_PRESENT       (1 << 7)
#define GDT_DPL_RING0     (0 << 5)
#define GDT_DPL_RING3     (3 << 5)
#define GDT_SEGMENT       (1 << 4)  /* S bit: code/data segment */
#define GDT_EXEC          (1 << 3)
#define GDT_READWRITE     (1 << 1)
#define GDT_ACCESSED      (1 << 0)

/* Granularity byte flags */
#define GDT_GRANULARITY_4K (1 << 7)
#define GDT_LONG_MODE      (1 << 5)  /* L bit */

/* ── GDT Descriptor (8 bytes) ─────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} gdt_entry_t;

/* ── TSS Descriptor (16 bytes — system descriptor type) ──────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_0_15;
    uint8_t  base_16_23;
    uint8_t  type;          /* 0x89 = present, ring 0, 64-bit available TSS */
    uint8_t  limit_high;
    uint8_t  base_24_31;
    uint32_t base_32_63;
    uint32_t reserved;
} tss_descriptor_t;

/* ── Task State Segment ───────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp[3];        /* RSP0–RSP2 (kernel stack pointers per ring) */
    uint64_t reserved1;
    uint64_t ist[7];        /* IST1–IST7 (interrupt stack tables) */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;    /* I/O permission bitmap offset */
} tss_t;

/* ── GDT pointer (for LGDT) ──────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} gdt_ptr_t;

/* ── Public API ───────────────────────────────────────────────────────────── */
void gdt_init(void);
void gdt_load_tss(void);
void tss_set_kernel_stack(uint64_t rsp0);

#endif /* ZIRVIUM_ARCH_X64_GDT_H */
