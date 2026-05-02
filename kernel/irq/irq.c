/* kernel/irq/irq.c
 * Zirvium Kernel — 8259A PIC + IRQ dispatch
 */
#include "irq.h"
#include "arch/x64/cpu.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── 8259A PIC I/O ports ──────────────────────────────────────────────────── */
#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

#define PIC_EOI    0x20   /* End-Of-Interrupt command */
#define PIC_READ_IRR 0x0A
#define PIC_READ_ISR 0x0B

/* ICW1 / ICW4 initialisation */
#define ICW1_INIT  0x11   /* cascade, edge, ICW4 needed */
#define ICW4_8086  0x01   /* 8086 mode, no auto-EOI */

/* IRQ base vectors after remapping */
#define PIC1_OFFSET  32
#define PIC2_OFFSET  40

/* ── Handler table ────────────────────────────────────────────────────────── */
#define MAX_IRQ_HANDLERS  16

typedef struct {
    irq_handler_t  fn;
    void          *data;
    const char    *name;
    unsigned long  flags;
} irq_entry_t;

/* Up to 4 shared handlers per IRQ line */
#define MAX_SHARED  4
static irq_entry_t irq_table[MAX_IRQ_HANDLERS][MAX_SHARED];

/* ── PIC initialisation ───────────────────────────────────────────────────── */
void irq_init(void)
{
    /* Save masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* ICW1: start initialisation */
    outb(PIC1_CMD, ICW1_INIT);
    outb(PIC2_CMD, ICW1_INIT);

    /* ICW2: vector offsets */
    outb(PIC1_DATA, PIC1_OFFSET);   /* IRQ 0-7  → vectors 32-39 */
    outb(PIC2_DATA, PIC2_OFFSET);   /* IRQ 8-15 → vectors 40-47 */

    /* ICW3: cascade wiring */
    outb(PIC1_DATA, 0x04);   /* slave on IRQ2 */
    outb(PIC2_DATA, 0x02);   /* slave ID = 2  */

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    /* Restore masks (all masked except cascade) */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);

    /* Unmask everything at init — drivers will mask what they don't need */
    outb(PIC1_DATA, 0x00);
    outb(PIC2_DATA, 0x00);

    memset(irq_table, 0, sizeof(irq_table));
}

/* ── Handler registration ─────────────────────────────────────────────────── */
int request_irq(int irq, irq_handler_t handler,
                unsigned long flags, const char *name, void *data)
{
    if (irq < 0 || irq >= MAX_IRQ_HANDLERS || !handler) return -1;
    for (int s = 0; s < MAX_SHARED; s++) {
        if (!irq_table[irq][s].fn) {
            irq_table[irq][s].fn    = handler;
            irq_table[irq][s].data  = data;
            irq_table[irq][s].name  = name;
            irq_table[irq][s].flags = flags;
            irq_unmask(irq);
            return 0;
        }
    }
    return -1;   /* no free slot */
}

void free_irq(int irq, void *data)
{
    if (irq < 0 || irq >= MAX_IRQ_HANDLERS) return;
    for (int s = 0; s < MAX_SHARED; s++) {
        if (irq_table[irq][s].data == data) {
            irq_table[irq][s].fn   = NULL;
            irq_table[irq][s].data = NULL;
        }
    }
}

/* ── PIC mask / unmask ────────────────────────────────────────────────────── */
void irq_mask(int irq)
{
    if (irq < 8) {
        uint8_t m = inb(PIC1_DATA) | (uint8_t)(1 << irq);
        outb(PIC1_DATA, m);
    } else {
        uint8_t m = inb(PIC2_DATA) | (uint8_t)(1 << (irq - 8));
        outb(PIC2_DATA, m);
    }
}

void irq_unmask(int irq)
{
    if (irq < 8) {
        uint8_t m = inb(PIC1_DATA) & (uint8_t)~(1 << irq);
        outb(PIC1_DATA, m);
    } else {
        uint8_t m = inb(PIC2_DATA) & (uint8_t)~(1 << (irq - 8));
        outb(PIC2_DATA, m);
        irq_unmask(IRQ_CASCADE);   /* make sure cascade is unmasked */
    }
}

void irq_eoi(int irq)
{
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

/* ── Dispatch ─────────────────────────────────────────────────────────────── */
void irq_dispatch(int vector)
{
    int irq = vector - PIC1_OFFSET;
    if (irq < 0 || irq >= MAX_IRQ_HANDLERS) {
        irq_eoi(0);
        return;
    }

    for (int s = 0; s < MAX_SHARED; s++) {
        if (irq_table[irq][s].fn)
            irq_table[irq][s].fn(irq, irq_table[irq][s].data);
    }

    irq_eoi(irq);
}
