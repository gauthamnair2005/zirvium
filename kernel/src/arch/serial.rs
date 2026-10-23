//! 16550-compatible serial console on COM1.
//!
//! Serial is the primary diagnostic channel: it works before the framebuffer
//! exists and survives a userspace crash.  The logger and panic handler write
//! here unconditionally.

use crate::arch::port::{inb, outb};
use core::fmt;
use zirvstdlib::sync::SpinLock;

const COM1: u16 = 0x3F8;

static SERIAL_LOCK: SpinLock<()> = SpinLock::new(());

pub fn init() {
    // SAFETY: COM1 is the standard PC serial port.  The sequence below is the
    // canonical 16550 initialisation: disable interrupts, set the divisor,
    // configure 8N1, enable and clear the FIFOs, then raise DTR/RTS.
    unsafe {
        outb(COM1 + 1, 0x00); // Disable interrupts.
        outb(COM1 + 3, 0x80); // Enable DLAB.
        outb(COM1 + 0, 0x01); // Divisor low: 115200 baud.
        outb(COM1 + 1, 0x00); // Divisor high.
        outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit.
        outb(COM1 + 2, 0xC7); // Enable FIFO, clear, 14-byte threshold.
        outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set.
        // Loopback self-test.
        outb(COM1 + 4, 0x1E);
        outb(COM1 + 0, 0xAE);
        if inb(COM1 + 0) != 0xAE {
            // The port is absent or broken.  Keep going; the framebuffer
            // console will still work.
            outb(COM1 + 4, 0x0B);
            return;
        }
        outb(COM1 + 4, 0x0B);
    }
}

#[inline]
fn line_status() -> u8 {
    // SAFETY: COM1 is a valid port after `init`.
    unsafe { inb(COM1 + 5) }
}

#[inline]
fn transmit_ready() -> bool {
    line_status() & 0x20 != 0
}

pub fn write_byte(byte: u8) {
    if byte == b'\n' {
        write_raw(b'\r');
    }
    write_raw(byte);
}

fn write_raw(byte: u8) {
    let _guard = SERIAL_LOCK.lock();
    // Wait for the transmitter to drain.  Bound the loop so a missing UART
    // cannot hang the kernel forever.
    for _ in 0..100_000 {
        if transmit_ready() {
            break;
        }
        core::hint::spin_loop();
    }
    // SAFETY: COM1 is a valid port after `init`.
    unsafe { outb(COM1, byte) };
}

pub fn write_str(text: &str) {
    for byte in text.bytes() {
        write_byte(byte);
    }
}

/// Write directly, ignoring the lock.  Used by the panic handler.
pub fn force_write_str(text: &str) {
    for byte in text.bytes() {
        if byte == b'\n' {
            force_write_raw(b'\r');
        }
        force_write_raw(byte);
    }
}

fn force_write_raw(byte: u8) {
    for _ in 0..100_000 {
        if transmit_ready() {
            break;
        }
    }
    // SAFETY: COM1 is a valid port after `init`.
    unsafe { outb(COM1, byte) };
}

pub fn print(arguments: fmt::Arguments<'_>) {
    let mut writer = SerialWriter;
    let _ = fmt::write(&mut writer, arguments);
}

pub struct SerialWriter;

impl fmt::Write for SerialWriter {
    fn write_str(&mut self, text: &str) -> fmt::Result {
        write_str(text);
        Ok(())
    }
}

#[macro_export]
macro_rules! sprint {
    ($($arg:tt)*) => {
        $crate::arch::serial::print(format_args!($($arg)*))
    };
}

#[macro_export]
macro_rules! sprintln {
    () => {
        $crate::arch::serial::write_str("\n")
    };
    ($($arg:tt)*) => {{
        $crate::arch::serial::print(format_args!($($arg)*));
        $crate::arch::serial::write_str("\n");
    }};
}
