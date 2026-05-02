/* arch/x64/idt.h
 * Zirvium Kernel — Interrupt Descriptor Table (x86-64)
 */
#ifndef ZIRVIUM_ARCH_X64_IDT_H
#define ZIRVIUM_ARCH_X64_IDT_H

#include <stdint.h>

/* Gate types */
#define IDT_GATE_INTERRUPT  0xE   /* 64-bit interrupt gate  (clears IF) */
#define IDT_GATE_TRAP       0xF   /* 64-bit trap gate       (keeps IF)  */

/* Privilege levels */
#define IDT_DPL_KERNEL      0
#define IDT_DPL_USER        3

/* Number of IDT entries */
#define IDT_ENTRIES         256

/* ── IDT Gate Descriptor ───────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t offset_low;    /* handler[15:0]  */
    uint16_t selector;      /* code segment selector */
    uint8_t  ist;           /* interrupt stack table index (0 = legacy) */
    uint8_t  type_attr;     /* type (4b) | 0 (1b) | DPL (2b) | P (1b) */
    uint16_t offset_mid;    /* handler[31:16] */
    uint32_t offset_high;   /* handler[63:32] */
    uint32_t reserved;
} idt_entry_t;

/* ── IDT Pointer (for LIDT) ───────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idt_ptr_t;

/* ── Saved CPU state pushed by the ISR stubs ─────────────────────────────── */
typedef struct __attribute__((packed)) {
    /* General-purpose registers (saved by stub) */
    uint64_t r15, r14, r13, r12, r11, r10, r9,  r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    /* Exception meta-data */
    uint64_t int_no;        /* interrupt / exception number */
    uint64_t err_code;      /* error code (0 if none) */
    /* Pushed by CPU on exception entry */
    uint64_t rip, cs, rflags, rsp, ss;
} cpu_state_t;

/* ── Public API ───────────────────────────────────────────────────────────── */
void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t sel,
                  uint8_t gate_type, uint8_t dpl);

/* Exported ISR / IRQ stubs (defined in arch/x64/isr_stubs.asm) */
extern void isr_stubs_init(void);   /* installs all 256 stubs */

#endif /* ZIRVIUM_ARCH_X64_IDT_H */
