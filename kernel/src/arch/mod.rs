//! x86-64 architecture support.

pub mod cpu;
pub mod gdt;
pub mod idt;
pub mod interrupts;
pub mod paging;
pub mod pic;
pub mod pit;
pub mod port;
pub mod serial;

/// Enable the architectural pieces every higher layer assumes.
pub fn init() {
    cpu::enable_sse();
    cpu::enable_avx();
    paging::init();
    gdt::init();
    idt::init();
    pic::remap();
    // Timer (IRQ0), keyboard (IRQ1) and the slave cascade (IRQ2).
    pic::unmask(0);
    pic::unmask(1);
    pic::unmask(2);
    pic::unmask(12);
    pit::init(250);
    // Interrupts stay disabled: the PS/2 controller is initialised by polling
    // command acknowledgements next, and enabling IRQs first would race with
    // those reads.  The boot path calls `cpu::sti()` afterwards.
}
