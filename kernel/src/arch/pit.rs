//! The 8253/8254 programmable interval timer.

use crate::arch::port::outb;
use core::sync::atomic::{AtomicU64, Ordering};

const PIT_FREQUENCY: u64 = 1_193_182;

static TICKS: AtomicU64 = AtomicU64::new(0);
static FREQUENCY: AtomicU64 = AtomicU64::new(0);

/// Program channel 0 to `hz` interrupts per second.
pub fn init(hz: u32) {
    let hz = hz.clamp(19, 10_000);
    let divisor = (PIT_FREQUENCY / hz as u64).clamp(1, 65_535) as u16;
    // SAFETY: PIT ports are standard on PC-compatible hardware.
    unsafe {
        outb(0x43, 0x36); // Channel 0, lo/hi byte, mode 3, binary.
        outb(0x40, divisor as u8);
        outb(0x40, (divisor >> 8) as u8);
    }
    FREQUENCY.store(hz as u64, Ordering::Relaxed);
    TICKS.store(0, Ordering::Relaxed);
    zirvstdlib::time::set_monotonic_ms(0);
}

/// Called from the timer interrupt.
pub fn tick() {
    let ticks = TICKS.fetch_add(1, Ordering::Relaxed) + 1;
    let hz = FREQUENCY.load(Ordering::Relaxed).max(1);
    zirvstdlib::time::set_monotonic_ms(ticks * 1000 / hz);
}

pub fn ticks() -> u64 {
    TICKS.load(Ordering::Relaxed)
}

pub fn frequency() -> u64 {
    FREQUENCY.load(Ordering::Relaxed).max(1)
}

pub fn uptime_ms() -> u64 {
    ticks() * 1000 / frequency()
}

/// Busy-wait for a number of milliseconds.  This is only used before the
/// scheduler exists; afterwards tasks should sleep instead.
pub fn spin_wait_ms(milliseconds: u64) {
    let start = ticks();
    let target = start + (milliseconds * frequency()).div_ceil(1000);
    while ticks() < target {
        core::hint::spin_loop();
    }
}
