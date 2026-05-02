/* kernel/irq/irq.h
 * Zirvium Kernel — IRQ subsystem
 *
 * Provides IRQ handler registration, PIC/APIC management, and IRQ routing.
 * Drivers call request_irq() to install their handler; the IDT dispatch
 * (isr_dispatch in kernel/main.c) calls irq_dispatch() for vectors ≥ 32.
 */
#ifndef ZIRVIUM_KERNEL_IRQ_IRQ_H
#define ZIRVIUM_KERNEL_IRQ_IRQ_H

#include <stdint.h>
#include <stdbool.h>

/* IRQ numbers (PIC-style): IRQ 0–15 map to IDT vectors 32–47 */
#define IRQ_TIMER       0
#define IRQ_KEYBOARD    1
#define IRQ_CASCADE     2   /* slave PIC cascade */
#define IRQ_COM2        3
#define IRQ_COM1        4
#define IRQ_LPT2        5
#define IRQ_FLOPPY      6
#define IRQ_LPT1        7
#define IRQ_RTC         8
#define IRQ_FREE9       9
#define IRQ_FREE10     10
#define IRQ_FREE11     11
#define IRQ_PS2_MOUSE  12
#define IRQ_FPU        13
#define IRQ_ATA_PRI    14
#define IRQ_ATA_SEC    15

/* IRQ_NONE is returned when no handler matches */
#define IRQ_NONE    0
#define IRQ_HANDLED 1

typedef int (*irq_handler_t)(int irq, void *data);

/* IRQ flags */
#define IRQF_SHARED     (1 << 0)   /* shared IRQ line allowed */
#define IRQF_TRIGGER_EDGE (1 << 1)

/**
 * irq_init - initialise the 8259A PIC, remapping IRQs 0–15 to vectors 32–47.
 *            Must be called before any request_irq().
 */
void irq_init(void);

/**
 * request_irq - install an IRQ handler.
 * @irq:     IRQ line (0–15 for PIC, or MSI vector)
 * @handler: callback; returns IRQ_HANDLED or IRQ_NONE
 * @flags:   IRQF_* flags
 * @name:    human-readable name (for /config/irqs listing)
 * @data:    opaque pointer passed back to handler
 *
 * Returns 0 on success, negative on error.
 */
int request_irq(int irq, irq_handler_t handler,
                unsigned long flags, const char *name, void *data);

/**
 * free_irq - remove a previously installed IRQ handler.
 */
void free_irq(int irq, void *data);

/**
 * irq_dispatch - called by the IDT exception handler for vectors 32–255.
 * @vector: IDT vector number
 */
void irq_dispatch(int vector);

/**
 * irq_mask / irq_unmask - mask or unmask a PIC IRQ line.
 */
void irq_mask(int irq);
void irq_unmask(int irq);

/**
 * irq_eoi - send End-Of-Interrupt to the PIC.
 * Must be called at the end of every hardware IRQ handler.
 */
void irq_eoi(int irq);

#endif /* ZIRVIUM_KERNEL_IRQ_IRQ_H */
