//! Kernel time helpers.

use zirvstdlib::time::DateTime;

pub use crate::arch::pit::uptime_ms;

/// Current wall-clock time.
pub fn now() -> DateTime {
    crate::drivers::rtc::now()
}

/// Unix seconds at boot.
pub fn boot_unix() -> i64 {
    crate::drivers::rtc::boot_unix()
}

/// A short uptime string such as `2m 14s`.
pub fn uptime_string() -> zirvstdlib::fmt::FixedString<32> {
    let total = uptime_ms() / 1000;
    let hours = total / 3_600;
    let minutes = (total / 60) % 60;
    let seconds = total % 60;
    let mut output = zirvstdlib::fmt::FixedString::<32>::new();
    use core::fmt::Write;
    if hours > 0 {
        let _ = write!(output, "{hours}h {minutes}m {seconds}s");
    } else if minutes > 0 {
        let _ = write!(output, "{minutes}m {seconds}s");
    } else {
        let _ = write!(output, "{seconds}s");
    }
    output
}
