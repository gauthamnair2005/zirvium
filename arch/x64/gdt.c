/* arch/x64/gdt.c
 * Zirvium Kernel — GDT + TSS initialisation
 */
#include "gdt.h"
#include <stdint.h>
#include <stddef.h>

/* ── Static tables ────────────────────────────────────────────────────────── */
/* 5 regular 8-byte descriptors + 1 TSS (16 bytes, counts as 2 slots) */
#define GDT_ENTRIES  7
static gdt_entry_t    gdt_table[GDT_ENTRIES];
static tss_descriptor_t *tss_desc;   /* pointer into gdt_table slot 5 */
static tss_t          tss;
static gdt_ptr_t      gdt_ptr;

/* ── Helpers ──────────────────────────────────────────────────────────────── */
static void set_entry(int idx, uint32_t base, uint32_t limit,
                      uint8_t access, uint8_t gran)
{
    gdt_table[idx].base_low   = (uint16_t)(base & 0xFFFF);
    gdt_table[idx].base_mid   = (uint8_t)((base >> 16) & 0xFF);
    gdt_table[idx].base_high  = (uint8_t)((base >> 24) & 0xFF);
    gdt_table[idx].limit_low  = (uint16_t)(limit & 0xFFFF);
    gdt_table[idx].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt_table[idx].access     = access;
}

static void set_tss_descriptor(uint64_t base, uint32_t limit)
{
    /* TSS occupies slots 5 and 6 (16 bytes total) */
    tss_desc = (tss_descriptor_t *)&gdt_table[5];

    tss_desc->limit_low  = (uint16_t)(limit & 0xFFFF);
    tss_desc->base_0_15  = (uint16_t)(base & 0xFFFF);
    tss_desc->base_16_23 = (uint8_t)((base >> 16) & 0xFF);
    tss_desc->type       = 0x89;   /* present, DPL 0, 64-bit available TSS */
    tss_desc->limit_high = (uint8_t)((limit >> 16) & 0x0F);
    tss_desc->base_24_31 = (uint8_t)((base >> 24) & 0xFF);
    tss_desc->base_32_63 = (uint32_t)(base >> 32);
    tss_desc->reserved   = 0;
}

/* External symbol — loads a GDT pointer and reloads segment registers.
 * Implemented as a small inline asm because we cannot use a far ret from C. */
static void gdt_flush(void)
{
    __asm__ volatile(
        "lgdt %0\n\t"
        /* Reload data segments */
        "mov %1, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        /* Far return to reload CS */
        "push %2\n\t"
        "lea 1f(%%rip), %%rax\n\t"
        "push %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        :
        : "m"(gdt_ptr),
          "i"(GDT_KERNEL_DATA),
          "i"((uint64_t)GDT_KERNEL_CODE)
        : "rax", "memory"
    );
}

/* ── Public API ───────────────────────────────────────────────────────────── */
void gdt_init(void)
{
    /* 0: null */
    set_entry(0, 0, 0, 0, 0);

    /* 1: kernel code (0x08) — long mode, DPL 0 */
    set_entry(1, 0, 0xFFFFF,
              GDT_PRESENT | GDT_DPL_RING0 | GDT_SEGMENT | GDT_EXEC | GDT_READWRITE,
              GDT_GRANULARITY_4K | GDT_LONG_MODE);

    /* 2: kernel data (0x10) — DPL 0 */
    set_entry(2, 0, 0xFFFFF,
              GDT_PRESENT | GDT_DPL_RING0 | GDT_SEGMENT | GDT_READWRITE,
              GDT_GRANULARITY_4K);

    /* 3: user code (0x18) — long mode, DPL 3 */
    set_entry(3, 0, 0xFFFFF,
              GDT_PRESENT | GDT_DPL_RING3 | GDT_SEGMENT | GDT_EXEC | GDT_READWRITE,
              GDT_GRANULARITY_4K | GDT_LONG_MODE);

    /* 4: user data (0x20) — DPL 3 */
    set_entry(4, 0, 0xFFFFF,
              GDT_PRESENT | GDT_DPL_RING3 | GDT_SEGMENT | GDT_READWRITE,
              GDT_GRANULARITY_4K);

    /* 5–6: TSS (0x28, 16-byte system descriptor) */
    set_tss_descriptor((uint64_t)&tss, sizeof(tss) - 1);
    tss.iomap_base = sizeof(tss);   /* no I/O permission bitmap */

    gdt_ptr.limit = (uint16_t)(sizeof(gdt_table) - 1);
    gdt_ptr.base  = (uint64_t)&gdt_table[0];

    gdt_flush();
}

void gdt_load_tss(void)
{
    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS_LOW));
}

void tss_set_kernel_stack(uint64_t rsp0)
{
    tss.rsp[0] = rsp0;
}
