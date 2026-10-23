//! x86-64 port I/O.

use core::arch::asm;

#[inline]
pub unsafe fn outb(port: u16, value: u8) {
    // SAFETY: the caller is responsible for using a valid port.
    unsafe {
        asm!("out dx, al", in("dx") port, in("al") value, options(nomem, nostack, preserves_flags));
    }
}

#[inline]
pub unsafe fn inb(port: u16) -> u8 {
    let value: u8;
    // SAFETY: the caller is responsible for using a valid port.
    unsafe {
        asm!("in al, dx", out("al") value, in("dx") port, options(nomem, nostack, preserves_flags));
    }
    value
}

#[inline]
pub unsafe fn outw(port: u16, value: u16) {
    // SAFETY: the caller is responsible for using a valid port.
    unsafe {
        asm!("out dx, ax", in("dx") port, in("ax") value, options(nomem, nostack, preserves_flags));
    }
}

#[inline]
pub unsafe fn inw(port: u16) -> u16 {
    let value: u16;
    // SAFETY: the caller is responsible for using a valid port.
    unsafe {
        asm!("in ax, dx", out("ax") value, in("dx") port, options(nomem, nostack, preserves_flags));
    }
    value
}

#[inline]
pub unsafe fn outl(port: u16, value: u32) {
    // SAFETY: the caller is responsible for using a valid port.
    unsafe {
        asm!("out dx, eax", in("dx") port, in("eax") value, options(nomem, nostack, preserves_flags));
    }
}

#[inline]
pub unsafe fn inl(port: u16) -> u32 {
    let value: u32;
    // SAFETY: the caller is responsible for using a valid port.
    unsafe {
        asm!("in eax, dx", out("eax") value, in("dx") port, options(nomem, nostack, preserves_flags));
    }
    value
}

/// A short delay for slow legacy devices.
#[inline]
pub unsafe fn io_wait() {
    // SAFETY: port 0x80 is the traditional diagnostic port and is always
    // safe to write on PC-compatible machines.
    unsafe { outb(0x80, 0) };
}
