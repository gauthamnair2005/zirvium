/* arch/x64/idt.c
 * Zirvium Kernel — IDT initialisation
 */
#include "idt.h"
#include <stdint.h>

/* ISR stub entry-points declared in isr_stubs.asm */
extern void *isr_stub_table[IDT_ENTRIES];

static idt_entry_t idt_entries[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

void idt_set_gate(uint8_t num, uint64_t handler, uint16_t sel,
                  uint8_t gate_type, uint8_t dpl)
{
    idt_entry_t *e = &idt_entries[num];

    e->offset_low  = (uint16_t)(handler & 0xFFFF);
    e->selector    = sel;
    e->ist         = 0;
    e->type_attr   = (uint8_t)((1 << 7) | ((dpl & 0x3) << 5) | (gate_type & 0xF));
    e->offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    e->offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    e->reserved    = 0;
}

/* Kernel code selector (matches GDT built in boot.asm) */
#define KERNEL_CS  0x08

void idt_init(void)
{
    /* Install all 256 stubs from the pre-built stub table */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate((uint8_t)i, (uint64_t)isr_stub_table[i],
                     KERNEL_CS, IDT_GATE_INTERRUPT, IDT_DPL_KERNEL);
    }

    idt_ptr.limit = (uint16_t)(sizeof(idt_entries) - 1);
    idt_ptr.base  = (uint64_t)&idt_entries[0];

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}
