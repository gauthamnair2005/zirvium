//! Global descriptor table, task state segment and ring-0 stacks.

use core::mem::size_of;

#[repr(C, packed)]
struct DescriptorTablePointer {
    limit: u16,
    base: u64,
}

/// The 64-bit Task State Segment.  Only `rsp0` and IST1 are used for now;
/// the remaining fields are required by the architecture.
#[repr(C, packed)]
struct Tss {
    reserved0: u32,
    rsp0: u64,
    rsp1: u64,
    rsp2: u64,
    reserved1: u64,
    ist: [u64; 7],
    reserved2: u64,
    reserved3: u16,
    iomap_base: u16,
}

impl Tss {
    const fn new() -> Self {
        Self {
            reserved0: 0,
            rsp0: 0,
            rsp1: 0,
            rsp2: 0,
            reserved1: 0,
            ist: [0; 7],
            reserved2: 0,
            reserved3: 0,
            iomap_base: size_of::<Tss>() as u16,
        }
    }
}

const GDT_ENTRIES: usize = 7;

#[repr(C, align(16))]
struct Gdt {
    entries: [u64; GDT_ENTRIES],
}

static mut GDT: Gdt = Gdt {
    entries: [0; GDT_ENTRIES],
};
static mut GDTR: DescriptorTablePointer = DescriptorTablePointer { limit: 0, base: 0 };
static mut TSS: Tss = Tss::new();

#[repr(C, align(16))]
struct Stack<const N: usize>([u8; N]);

static mut INTERRUPT_STACK: Stack<16_384> = Stack([0; 16_384]);
static mut DOUBLE_FAULT_STACK: Stack<8_192> = Stack([0; 8_192]);

pub const KERNEL_CODE: u16 = 0x08;
pub const KERNEL_DATA: u16 = 0x10;
pub const USER_CODE: u16 = 0x18 | 3;
pub const USER_DATA: u16 = 0x20 | 3;
pub const TSS_SELECTOR: u16 = 0x28;

core::arch::global_asm!(
    r#"
    .section .text
    .global zirv_load_gdt
    zirv_load_gdt:
        lgdt [rdi]
        push 0x08
        lea rax, [rip + 2f]
        push rax
        retfq
    2:
        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov fs, ax
        mov gs, ax
        ret
    "#
);

unsafe extern "sysv64" {
    fn zirv_load_gdt(pointer: *const DescriptorTablePointer);
}

/// Install the kernel GDT and TSS.  Must be called with interrupts disabled.
pub fn init() {
    // SAFETY: this function runs exactly once during early boot with a single
    // CPU and interrupts disabled.
    unsafe {
        let gdt = &raw mut GDT;
        let entries = &mut (*gdt).entries;
        entries[0] = 0;
        entries[1] = 0x00AF_9A00_0000_FFFF; // Kernel code, ring 0.
        entries[2] = 0x00AF_9200_0000_FFFF; // Kernel data, ring 0.
        entries[3] = 0x00AF_FA00_0000_FFFF; // User code, ring 3.
        entries[4] = 0x00AF_F200_0000_FFFF; // User data, ring 3.

        let tss = &raw mut TSS;
        let stack = &raw mut INTERRUPT_STACK;
        let df_stack = &raw mut DOUBLE_FAULT_STACK;
        let rsp0 = (*stack).0.as_mut_ptr().add(16_384) as u64;
        let ist1 = (*df_stack).0.as_mut_ptr().add(8_192) as u64;
        (*tss).rsp0 = rsp0;
        (*tss).ist[0] = ist1;
        (*tss).iomap_base = size_of::<Tss>() as u16;

        let base = tss as u64;
        let limit = (size_of::<Tss>() - 1) as u64;
        entries[5] = (limit & 0xFFFF)
            | ((base & 0xFF_FFFF) << 16)
            | (0x89u64 << 40)
            | (((limit >> 16) & 0xF) << 48)
            | (((base >> 24) & 0xFF) << 56);
        entries[6] = base >> 32;

        let gdtr = &raw mut GDTR;
        (*gdtr).limit = (GDT_ENTRIES * 8 - 1) as u16;
        (*gdtr).base = gdt as u64;
        zirv_load_gdt(gdtr);

        core::arch::asm!("ltr ax", in("ax") TSS_SELECTOR, options(nomem, nostack));
    }
}

/// The top of the interrupt stack.
pub fn interrupt_stack_top() -> u64 {
    // SAFETY: the stack is a static object.
    unsafe { (*core::ptr::addr_of_mut!(INTERRUPT_STACK)).0.as_mut_ptr().add(16_384) as u64 }
}
