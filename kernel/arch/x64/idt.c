#include <arch/x64.h>
#include <kernel/kernel.h>

static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

extern void idt_flush(uint64_t);
extern void isr_stub_0(void);

static void idt_set_gate(int num, uint64_t handler, uint16_t selector, uint8_t type_attr) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_mid = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].selector = selector;
    idt[num].ist = 0;
    idt[num].type_attr = type_attr;
    idt[num].zero = 0;
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint64_t)isr_stub_0, 0x08, 0x8E);
    }
    
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;
    
    idt_flush((uint64_t)&idt_ptr);
}

void interrupts_enable(void) {
    asm volatile("sti");
}

void interrupts_disable(void) {
    asm volatile("cli");
}
