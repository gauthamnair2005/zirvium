//! The interrupt descriptor table.

use crate::arch::gdt::KERNEL_CODE;
use crate::arch::interrupts;

#[derive(Clone, Copy)]
#[repr(C)]
struct IdtEntry {
    offset_low: u16,
    selector: u16,
    ist: u8,
    type_attr: u8,
    offset_mid: u16,
    offset_high: u32,
    reserved: u32,
}

impl IdtEntry {
    const fn empty() -> Self {
        Self {
            offset_low: 0,
            selector: 0,
            ist: 0,
            type_attr: 0,
            offset_mid: 0,
            offset_high: 0,
            reserved: 0,
        }
    }

    fn set(&mut self, handler: u64, ist: u8, dpl: u8) {
        self.offset_low = handler as u16;
        self.selector = KERNEL_CODE;
        self.ist = ist & 0x7;
        // Present, 64-bit interrupt gate, DPL.
        self.type_attr = 0x8E | ((dpl & 0x3) << 5);
        self.offset_mid = (handler >> 16) as u16;
        self.offset_high = (handler >> 32) as u32;
        self.reserved = 0;
    }
}

#[repr(C, align(16))]
struct Idt {
    entries: [IdtEntry; 256],
}

static mut IDT: Idt = Idt {
    entries: [IdtEntry::empty(); 256],
};

#[repr(C, packed)]
struct DescriptorTablePointer {
    limit: u16,
    base: u64,
}

/// Install the IDT and route the legacy PIC vectors.
pub fn init() {
    // SAFETY: this runs once during early boot with interrupts disabled.
    unsafe {
        let idt = &raw mut IDT;
        for vector in 0..interrupts::EXCEPTION_COUNT {
            let ist = if vector == 8 { 1 } else { 0 };
            (*idt).entries[vector].set(interrupts::isr_stub(vector), ist, 0);
        }
        for irq in 0..interrupts::IRQ_COUNT {
            let vector = 32 + irq;
            (*idt).entries[vector].set(interrupts::irq_stub(irq), 0, 0);
        }
        (*idt).entries[crate::syscall::VECTOR as usize].set(
            interrupts::syscall_stub_address(),
            0,
            3,
        );

        let pointer = DescriptorTablePointer {
            limit: (core::mem::size_of::<Idt>() - 1) as u16,
            base: idt as u64,
        };
        core::arch::asm!(
            "lidt [{}]",
            in(reg) &pointer,
            options(readonly, nostack, preserves_flags)
        );
    }
}
