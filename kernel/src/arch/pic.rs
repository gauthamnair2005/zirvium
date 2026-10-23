//! The legacy 8259 programmable interrupt controller.
//!
//! Zirvium remaps the PIC to vectors 32..48 and masks everything it does not
//! explicitly enable.  A future milestone will replace this with the local
//! APIC, but the PIC is the correct choice for a single-CPU bring-up.

use crate::arch::port::{inb, io_wait, outb};

const PIC1_COMMAND: u16 = 0x20;
const PIC1_DATA: u16 = 0x21;
const PIC2_COMMAND: u16 = 0xA0;
const PIC2_DATA: u16 = 0xA1;

const ICW1_INIT: u8 = 0x11;
const ICW4_8086: u8 = 0x01;

const PIC1_OFFSET: u8 = 32;
const PIC2_OFFSET: u8 = 40;

pub fn remap() {
    // SAFETY: these are the standard PIC ports on PC-compatible hardware.
    unsafe {
        outb(PIC1_COMMAND, ICW1_INIT);
        io_wait();
        outb(PIC2_COMMAND, ICW1_INIT);
        io_wait();
        outb(PIC1_DATA, PIC1_OFFSET);
        io_wait();
        outb(PIC2_DATA, PIC2_OFFSET);
        io_wait();
        outb(PIC1_DATA, 0x04); // Slave is on IRQ2.
        io_wait();
        outb(PIC2_DATA, 0x02); // Slave identity.
        io_wait();
        outb(PIC1_DATA, ICW4_8086);
        io_wait();
        outb(PIC2_DATA, ICW4_8086);
        io_wait();
        outb(PIC1_DATA, 0xFF);
        outb(PIC2_DATA, 0xFF);
    }
}

/// Unmask an IRQ line (0..15).
pub fn unmask(irq: u8) {
    // SAFETY: PIC ports are standard.
    unsafe {
        if irq < 8 {
            let value = inb(PIC1_DATA) & !(1u8 << irq);
            outb(PIC1_DATA, value);
        } else {
            let value = inb(PIC2_DATA) & !(1u8 << (irq - 8));
            outb(PIC2_DATA, value);
            let master = inb(PIC1_DATA) & !(1u8 << 2);
            outb(PIC1_DATA, master);
        }
    }
}

/// Mask an IRQ line.
pub fn mask(irq: u8) {
    // SAFETY: PIC ports are standard.
    unsafe {
        if irq < 8 {
            let value = inb(PIC1_DATA) | (1u8 << irq);
            outb(PIC1_DATA, value);
        } else {
            let value = inb(PIC2_DATA) | (1u8 << (irq - 8));
            outb(PIC2_DATA, value);
        }
    }
}

/// Send an end-of-interrupt for a vector in the remapped range.
pub fn eoi(vector: u8) {
    // SAFETY: PIC ports are standard.
    unsafe {
        if vector >= PIC2_OFFSET {
            outb(PIC2_COMMAND, 0x20);
        }
        outb(PIC1_COMMAND, 0x20);
    }
}

/// Disable the PIC entirely.  Used if we later switch to the APIC.
pub fn disable() {
    // SAFETY: PIC ports are standard.
    unsafe {
        outb(PIC1_DATA, 0xFF);
        outb(PIC2_DATA, 0xFF);
    }
}
