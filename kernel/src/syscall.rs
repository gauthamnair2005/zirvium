//! The Zirvium system call gate.
//!
//! `int 0x80` enters through the interrupt gate installed by `arch::idt`.
//! The stub saves every general-purpose register, so the ABI is simply:
//!
//! * `rax` — call number (see [`zirvstdlib::abi::syscall`])
//! * `rdi`, `rsi`, `rdx` — arguments
//! * `rax` on return — result
//!
//! There is no userspace yet, so the handler treats pointers as kernel
//! addresses.  When ring 3 arrives the range check in [`user_slice`] becomes
//! the copy-in/copy-out boundary.

use zirvstdlib::abi::syscall as number;

/// Interrupt vector used for system calls.
pub const VECTOR: u64 = 0x80;

/// Register indices inside the frame built by `irq_common`.
const R15: usize = 0;
const RDI: usize = 8;
const RSI: usize = 9;
const RDX: usize = 12;
const RAX: usize = 14;

/// Handle a system call from the interrupt dispatcher.
///
/// # Safety
/// `frame` must point at the register block pushed by `irq_common`.
pub unsafe fn dispatch(frame: *mut u64) {
    if frame.is_null() {
        return;
    }
    // SAFETY: the caller guarantees a 15-entry register block.
    unsafe {
        let call = frame.add(RAX).read();
        let argument0 = frame.add(RDI).read();
        let argument1 = frame.add(RSI).read();
        let argument2 = frame.add(RDX).read();
        let result = handle(call, argument0, argument1, argument2);
        frame.add(RAX).write(result);
    }
}

fn handle(call: u64, argument0: u64, argument1: u64, argument2: u64) -> u64 {
    match call {
        number::PING => number::PONG,
        number::WRITE => write(argument0, argument1),
        number::GETPID => crate::mosix::desktop_pid() as u64,
        number::UPTIME_MS => crate::arch::pit::uptime_ms(),
        number::NODE_COUNT => crate::mosix::node_count() as u64,
        number::SPAWN => spawn(argument0, argument1, argument2),
        number::MIGRATE => {
            let pid = argument0 as u32;
            let node = argument1 as u32;
            u64::from(crate::mosix::migrate(pid, node))
        }
        number::MEMORY_FREE_KIB => crate::mm::stats().free_bytes / 1024,
        number::GPU_VENDOR => crate::drivers::pci::gpu().vendor as u64,
        number::PROCESS_COUNT => crate::mosix::process_count() as u64,
        number::LOAD_PERMILLE => crate::mosix::load_permille() as u64,
        number::TICKS => crate::arch::pit::ticks(),
        _ => number::UNKNOWN,
    }
}

fn write(pointer: u64, length: u64) -> u64 {
    let Some(bytes) = (unsafe { user_slice(pointer, length) }) else {
        return u64::MAX;
    };
    if let Ok(text) = core::str::from_utf8(bytes) {
        crate::arch::serial::write_str(text);
        length
    } else {
        u64::MAX
    }
}

fn spawn(name_pointer: u64, name_length: u64, memory_kib: u64) -> u64 {
    let Some(bytes) = (unsafe { user_slice(name_pointer, name_length) }) else {
        return 0;
    };
    let Ok(name) = core::str::from_utf8(bytes) else {
        return 0;
    };
    crate::mosix::spawn_process(crate::mosix::desktop_pid(), name, memory_kib) as u64
}

/// Validate a userspace pointer range.  Today every process is ring 0, so the
/// check only rejects null, wildly out-of-range and unreasonably large
/// requests.
///
/// # Safety
/// The caller must ensure the range is readable *for the current address
/// space*.  With a single identity-mapped kernel address space this is true
/// for every mapped address below [`crate::arch::paging::MAPPED_SIZE`].
unsafe fn user_slice<'a>(pointer: u64, length: u64) -> Option<&'a [u8]> {
    if pointer == 0 || length == 0 || length > 64 * 1024 {
        return None;
    }
    let end = pointer.checked_add(length)?;
    if end > crate::arch::paging::MAPPED_SIZE {
        return None;
    }
    // SAFETY: delegated to the caller; the range is inside the identity map.
    Some(unsafe { core::slice::from_raw_parts(pointer as *const u8, length as usize) })
}

/// Exercise the gate once during boot.  This proves that the IDT entry, the
/// assembly stub and the register hand-off all agree before the desktop starts
/// using them.
pub fn self_test() {
    let result: u64;
    // SAFETY: the gate is installed by the time this runs.  `int 0x80` saves
    // and restores all registers through the stub.
    unsafe {
        core::arch::asm!(
            "int 0x80",
            inout("rax") number::PING => result,
            in("rdi") 0u64,
            in("rsi") 0u64,
            in("rdx") 0u64,
            options(nostack)
        );
    }
    if result == number::PONG {
        crate::sprintln!("SYSCALL: int 0x80 gate operational");
    } else {
        crate::sprintln!(
            "SYSCALL: gate returned {result:#x}, expected {:#x}",
            number::PONG
        );
    }
}

const _: () = assert!(RAX < 15);
const _: () = assert!(R15 == 0);
