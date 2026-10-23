//! Interrupt and exception entry stubs.
//!
//! The stubs are the only assembly in this module.  Everything else is Rust.

use crate::arch::{cpu, pic, pit};

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct InterruptFrame {
    pub vector: u64,
    pub error: u64,
    pub rip: u64,
    pub cs: u64,
    pub rflags: u64,
    pub rsp: u64,
    pub ss: u64,
}

pub const EXCEPTION_COUNT: usize = 32;
pub const IRQ_COUNT: usize = 16;

core::arch::global_asm!(
    r#"
    .section .text

    .macro ISR_NOERR num
    .global isr_\num
    isr_\num:
        push 0
        push \num
        jmp isr_common
    .endm

    .macro ISR_ERR num
    .global isr_\num
    isr_\num:
        push \num
        jmp isr_common
    .endm

    ISR_NOERR 0
    ISR_NOERR 1
    ISR_NOERR 2
    ISR_NOERR 3
    ISR_NOERR 4
    ISR_NOERR 5
    ISR_NOERR 6
    ISR_NOERR 7
    ISR_ERR   8
    ISR_NOERR 9
    ISR_ERR   10
    ISR_ERR   11
    ISR_ERR   12
    ISR_ERR   13
    ISR_ERR   14
    ISR_NOERR 15
    ISR_NOERR 16
    ISR_ERR   17
    ISR_NOERR 18
    ISR_NOERR 19
    ISR_NOERR 20
    ISR_ERR   21
    ISR_NOERR 22
    ISR_NOERR 23
    ISR_NOERR 24
    ISR_NOERR 25
    ISR_NOERR 26
    ISR_NOERR 27
    ISR_NOERR 28
    ISR_ERR   29
    ISR_ERR   30
    ISR_NOERR 31

    isr_common:
        sub rsp, 8
        mov rdi, rsp
        add rdi, 8
        call exception_dispatch
        add rsp, 8
        add rsp, 16
        iretq

    .macro IRQ_STUB num, vector
    .global irq_\num
    irq_\num:
        push 0
        push \vector
        jmp irq_common
    .endm

    IRQ_STUB 0, 32
    IRQ_STUB 1, 33
    IRQ_STUB 2, 34
    IRQ_STUB 3, 35
    IRQ_STUB 4, 36
    IRQ_STUB 5, 37
    IRQ_STUB 6, 38
    IRQ_STUB 7, 39
    IRQ_STUB 8, 40
    IRQ_STUB 9, 41
    IRQ_STUB 10, 42
    IRQ_STUB 11, 43
    IRQ_STUB 12, 44
    IRQ_STUB 13, 45
    IRQ_STUB 14, 46
    IRQ_STUB 15, 47

    .global syscall_stub
    syscall_stub:
        push 0
        push 0x80
        jmp irq_common

    irq_common:
        push rax
        push rcx
        push rdx
        push rbx
        push rbp
        push rsi
        push rdi
        push r8
        push r9
        push r10
        push r11
        push r12
        push r13
        push r14
        push r15
        mov rdi, rsp
        mov rsi, [rsp + 120]
        call irq_dispatch
        pop r15
        pop r14
        pop r13
        pop r12
        pop r11
        pop r10
        pop r9
        pop r8
        pop rdi
        pop rsi
        pop rbp
        pop rbx
        pop rdx
        pop rcx
        pop rax
        add rsp, 16
        iretq

    .section .rodata
    .align 8
    .global isr_stub_table
    isr_stub_table:
        .quad isr_0, isr_1, isr_2, isr_3, isr_4, isr_5, isr_6, isr_7
        .quad isr_8, isr_9, isr_10, isr_11, isr_12, isr_13, isr_14, isr_15
        .quad isr_16, isr_17, isr_18, isr_19, isr_20, isr_21, isr_22, isr_23
        .quad isr_24, isr_25, isr_26, isr_27, isr_28, isr_29, isr_30, isr_31
    .global irq_stub_table
    irq_stub_table:
        .quad irq_0, irq_1, irq_2, irq_3, irq_4, irq_5, irq_6, irq_7
        .quad irq_8, irq_9, irq_10, irq_11, irq_12, irq_13, irq_14, irq_15
    "#
);

unsafe extern "sysv64" {
    static isr_stub_table: [u64; EXCEPTION_COUNT];
    static irq_stub_table: [u64; IRQ_COUNT];
    fn syscall_stub();
}

pub fn isr_stub(vector: usize) -> u64 {
    // SAFETY: the table is created by the linker and has exactly 32 entries.
    unsafe { *isr_stub_table.get_unchecked(vector) }
}

pub fn irq_stub(vector: usize) -> u64 {
    // SAFETY: the table is created by the linker and has exactly 16 entries.
    unsafe { *irq_stub_table.get_unchecked(vector) }
}

pub fn syscall_stub_address() -> u64 {
    let stub: unsafe extern "sysv64" fn() = syscall_stub;
    stub as usize as u64
}

const EXCEPTION_NAMES: [&str; 32] = [
    "divide error",
    "debug",
    "non-maskable interrupt",
    "breakpoint",
    "overflow",
    "bound range exceeded",
    "invalid opcode",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid TSS",
    "segment not present",
    "stack-segment fault",
    "general protection fault",
    "page fault",
    "reserved",
    "x87 floating-point exception",
    "alignment check",
    "machine check",
    "SIMD floating-point exception",
    "virtualisation exception",
    "control protection exception",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "hypervisor injection exception",
    "VMM communication exception",
    "security exception",
    "reserved",
];

/// Called from the assembly stubs with a pointer to the CPU frame.
#[unsafe(no_mangle)]
pub extern "sysv64" fn exception_dispatch(frame: *const InterruptFrame) -> ! {
    // SAFETY: the stub passes a pointer to a frame it has just built on the
    // current stack.
    let frame = unsafe { &*frame };
    let vector = frame.vector as usize;
    let name = EXCEPTION_NAMES.get(vector).copied().unwrap_or("unknown");
    crate::sprintln!(
        "\n!! CPU exception {vector} ({name}) error={:#x} rip={:#x} cs={:#x} rflags={:#x}",
        frame.error,
        frame.rip,
        frame.cs,
        frame.rflags
    );
    if vector == 14 {
        crate::sprintln!("   page fault address: {:#x}", cpu::read_cr2());
    }
    crate::arch::serial::force_write_str("!! system halted\n");
    cpu::cli();
    cpu::hlt_loop()
}

/// Called from the assembly IRQ stubs.
///
/// `frame` points at the saved general-purpose registers; `rax` is at
/// `frame[14]`, so the syscall handler can return a value by writing there.
#[unsafe(no_mangle)]
pub extern "sysv64" fn irq_dispatch(frame: *mut u64, vector: u64) {
    match vector {
        32 => pit::tick(),
        33 => crate::drivers::ps2::handle_keyboard(),
        34 => {
            // Cascade: the slave PIC raised an IRQ.  Nothing to do here; the
            // slave's own IRQ will follow.
        }
        44 => crate::drivers::ps2::handle_mouse(),
        crate::syscall::VECTOR => unsafe { crate::syscall::dispatch(frame) },
        _ => {}
    }
    if (32..48).contains(&vector) {
        pic::eoi(vector as u8);
    }
}
