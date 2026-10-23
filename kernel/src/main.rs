//! Zirvium 2.0 — kernel entry point.
//!
//! The firmware loads this PE32+ image, calls [`efi_main`] and hands us a
//! system table.  From there the path is:
//!
//! 1. bring up the serial console and the kernel logger;
//! 2. capture the GOP framebuffer, the memory map and the image range;
//! 3. switch to a static kernel stack;
//! 4. call `ExitBootServices` and never touch firmware code again;
//! 5. build real page tables and install the kernel heap;
//! 6. initialise the architectural layer (GDT, IDT, PIC, PIT) with
//!    interrupts still disabled;
//! 7. enumerate PCI, read the RTC and bring up PS/2;
//! 8. parse the embedded font atlas and create the DisplayJet surface;
//! 9. start MOSIX, prove the syscall gate works, enable interrupts and hand
//!    control to the desktop, which never returns.
//!
//! There is no C anywhere in this path, and no 32-bit stage.

#![no_std]
#![no_main]
// The ABI, driver and UI modules deliberately expose a wider surface than the
// current callers use; they are the contract for the next milestones.
#![allow(dead_code)]

extern crate alloc;

mod arch;
mod displayjet;
mod drivers;
mod input;
mod logger;
mod mm;
mod mosix;
mod syscall;
mod time;
mod uefi;
mod ui;

use core::ffi::c_void;
use core::ptr;
use core::sync::atomic::{AtomicU64, Ordering};

use zirvstdlib::sync::SpinLock;

use crate::uefi::BootInfo;

// ---------------------------------------------------------------------------
// Allocator
// ---------------------------------------------------------------------------

/// The kernel heap.  It is empty until `mm::init_heap` runs; `KernelAllocator`
/// fails cleanly on an empty heap, so early allocations return null rather
/// than faulting.
#[global_allocator]
static ALLOCATOR: mm::heap::KernelAllocator = mm::heap::KernelAllocator;

// ---------------------------------------------------------------------------
// Kernel stack
// ---------------------------------------------------------------------------

/// Kernel stack used from the moment we leave the firmware stack.  256 KiB is
/// generous for interrupt frames, the compositor and the desktop.
const KERNEL_STACK_SIZE: usize = 256 * 1024;

#[repr(C, align(16))]
struct KernelStack([u8; KERNEL_STACK_SIZE]);

static mut BOOT_STACK: KernelStack = KernelStack([0; KERNEL_STACK_SIZE]);

/// The stack pointer we are running on, published for the frame allocator.
static STACK_POINTER: AtomicU64 = AtomicU64::new(0);

// Enter the kernel on a fresh stack.
//
// The arguments arrive in the SysV registers: RDI is the stack, RSI the entry
// point, RDX the image handle and RCX the system table.  We jump rather than
// call: the kernel never returns to firmware, and the firmware stack must
// stay untouched in case the firmware needs it while `ExitBootServices`
// runs.  RSP is set eight bytes below the 16-byte-aligned top so that the
// callee sees the same alignment as after a `call`.
core::arch::global_asm!(
    r#"
    .section .text
    .global zirvium_enter_kernel
    zirvium_enter_kernel:
        mov r10, rsi
        mov rsp, rdi
        mov rdi, rdx
        mov rsi, rcx
        xor rbp, rbp
        jmp r10
        ud2
    "#
);

unsafe extern "sysv64" {
    fn zirvium_enter_kernel(stack: usize, entry: usize, arg0: usize, arg1: usize) -> !;
}

// ---------------------------------------------------------------------------
// Boot state
// ---------------------------------------------------------------------------

/// The boot information captured before `ExitBootServices`.
static mut BOOT_INFO: Option<BootInfo> = None;

/// Which phase of boot failed, for post-mortem inspection.
static BOOT_PHASE: SpinLock<&str> = SpinLock::new("firmware");

fn set_phase(phase: &'static str) {
    *BOOT_PHASE.lock() = phase;
}

fn boot_phase() -> &'static str {
    *BOOT_PHASE.lock()
}

/// Borrow the captured boot information.
///
/// # Safety
/// Only called after `BOOT_INFO` has been written, from the single kernel
/// thread of control.
fn boot_info() -> &'static BootInfo {
    // SAFETY: `BOOT_INFO` is written once during `efi_main` and read only
    // afterwards, from a single thread of control.
    unsafe { (*ptr::addr_of!(BOOT_INFO)).as_ref().unwrap() }
}

fn boot_info_mut() -> &'static mut BootInfo {
    // SAFETY: as above; the caller holds the only reference.
    unsafe { (*ptr::addr_of_mut!(BOOT_INFO)).as_mut().unwrap() }
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

/// UEFI entry point.  `image_handle` identifies this loaded image and
/// `system_table` is the firmware's service table.
#[unsafe(no_mangle)]
pub extern "efiapi" fn efi_main(image_handle: *mut c_void, system_table: *mut c_void) -> usize {
    arch::serial::init();
    crate::sprintln!();
    crate::sprintln!("Zirvium 2.0 — 64-bit Rust and assembly");
    crate::sprintln!("----------------------------------------");
    logger::init();

    let stack_top = boot_stack_top();
    STACK_POINTER.store(stack_top as u64, Ordering::Relaxed);

    crate::sprintln!("BOOT: firmware stack -> static stack at {:#x}", stack_top);

    // SAFETY: the boot stack is statically allocated, aligned and never
    // reused.  `kernel_main` is `extern "sysv64"` and never returns.
    unsafe {
        zirvium_enter_kernel(
            stack_top - 8,
            kernel_main as extern "sysv64" fn(*mut c_void, *mut c_void) -> ! as usize,
            image_handle as usize,
            system_table as usize,
        )
    }
}

fn boot_stack_top() -> usize {
    // SAFETY: taking the address of a `static mut` is always sound; the
    // resulting pointer is used as the initial stack pointer.
    unsafe { ptr::addr_of_mut!(BOOT_STACK.0) as usize + KERNEL_STACK_SIZE }
}

extern "sysv64" fn kernel_main(image_handle: *mut c_void, system_table: *mut c_void) -> ! {
    set_phase("uefi");
    // SAFETY: the firmware passed a valid system table; this runs in the
    // thread the firmware started.
    let info = uefi::init(image_handle, system_table as *mut uefi::SystemTable);
    let framebuffer = info.framebuffer;
    crate::sprintln!(
        "GOP: {}x{} stride {} at {:#x}",
        framebuffer.width,
        framebuffer.height,
        framebuffer.stride,
        framebuffer.address as u64
    );
    if !framebuffer.is_valid() {
        serial_halt("no framebuffer: GOP did not provide a usable scanout");
    }

    // Publish the boot information before we give up firmware services.
    // SAFETY: single-threaded, and nothing else reads `BOOT_INFO` yet.
    unsafe {
        *ptr::addr_of_mut!(BOOT_INFO) = Some(info);
    }

    set_phase("exit-boot-services");
    uefi::exit_boot_services(boot_info_mut());

    set_phase("memory");
    let stack_pointer = STACK_POINTER.load(Ordering::Relaxed);
    mm::init(boot_info(), stack_pointer);
    mm::init_heap();

    set_phase("arch");
    arch::init();

    set_phase("drivers");
    drivers::init_early();
    let gpu = drivers::pci::gpu();
    crate::sprintln!("PCI: display backend {:?}", gpu.driver);

    set_phase("display");
    let boot = boot_info();
    if boot.framebuffer.address as u64 + boot.framebuffer.size < arch::paging::MAPPED_SIZE {
        // The scanout is MMIO: writing through it must not be cached.
        arch::paging::mark_uncached(
            boot.framebuffer.address as u64,
            boot.framebuffer.address as u64 + boot.framebuffer.size,
        );
    }
    displayjet::font::init();
    let Some(mut display) = displayjet::Display::new(framebuffer) else {
        serial_halt("DisplayJet could not claim the framebuffer");
    };
    display.set_gpu(gpu);
    drivers::init_late(display.width, display.height);

    ui::splash(&mut display);

    set_phase("cluster");
    let logical = arch::cpu::logical_processors();
    let memory_mib = (mm::stats().total_bytes / (1024 * 1024)) as u32;
    mosix::init(logical, memory_mib);

    set_phase("syscall");
    syscall::self_test();

    set_phase("desktop");
    crate::sprintln!("BOOT: complete, starting the desktop");
    // Interrupts are enabled inside the desktop loop.
    ui::run(&mut display)
}

// ---------------------------------------------------------------------------
// Failure and panic handling
// ---------------------------------------------------------------------------

fn serial_halt(message: &str) -> ! {
    crate::sprintln!("BOOT FAILURE ({}): {message}", boot_phase());
    loop {
        arch::cpu::cli();
        arch::cpu::halt();
    }
}

#[panic_handler]
fn panic(info: &core::panic::PanicInfo<'_>) -> ! {
    arch::cpu::cli();
    crate::arch::serial::force_write_str("\n*** KERNEL PANIC *** phase=");
    crate::arch::serial::force_write_str(boot_phase());
    crate::arch::serial::force_write_str(" at ");
    if let Some(location) = info.location() {
        use core::fmt::Write;
        let mut writer = crate::arch::serial::SerialWriter;
        let _ = write!(
            writer,
            "{}:{}:{}",
            location.file(),
            location.line(),
            location.column()
        );
    } else {
        crate::arch::serial::force_write_str("<unknown>");
    }
    crate::arch::serial::force_write_str(": ");
    use core::fmt::Write;
    let mut writer = crate::arch::serial::SerialWriter;
    let _ = write!(writer, "{info}");
    crate::arch::serial::force_write_str("\n");

    loop {
        arch::cpu::halt();
    }
}
