//! CMOS real-time clock.
//!
//! The RTC is read once at boot to establish wall-clock time.  Afterwards the
//! monotonic PIT clock advances it, so interrupts do not need to keep polling
//! the CMOS (which is slow and can tear).

use crate::arch::port::{inb, outb};
use zirvstdlib::sync::Once;
use zirvstdlib::time::DateTime;

const CMOS_ADDRESS: u16 = 0x70;
const CMOS_DATA: u16 = 0x71;

static BOOT_UNIX: Once<i64> = Once::new();

fn read_register(register: u8) -> u8 {
    // SAFETY: the CMOS index and data ports are standard on PC hardware.
    unsafe {
        outb(CMOS_ADDRESS, register);
        inb(CMOS_DATA)
    }
}

fn update_in_progress() -> bool {
    read_register(0x0A) & 0x80 != 0
}

fn bcd_to_binary(value: u8) -> u8 {
    (value & 0x0F) + ((value >> 4) * 10)
}

/// Read the current wall-clock time from the RTC.
pub fn read() -> DateTime {
    // Wait for the update-in-progress flag to clear so the registers describe
    // a single instant.
    let mut guard = 0;
    while update_in_progress() && guard < 1_000_000 {
        guard += 1;
        core::hint::spin_loop();
    }

    let second = read_register(0x00);
    let minute = read_register(0x02);
    let hour = read_register(0x04);
    let day = read_register(0x07);
    let month = read_register(0x08);
    let year = read_register(0x09);
    let century = read_register(0x32);
    let status_b = read_register(0x0B);

    let binary = status_b & 0x04 != 0;
    let is_24h = status_b & 0x02 != 0;

    let mut second = if binary {
        second
    } else {
        bcd_to_binary(second)
    };
    let mut minute = if binary {
        minute
    } else {
        bcd_to_binary(minute)
    };
    let mut hour_raw = if binary { hour } else { bcd_to_binary(hour) };
    let pm = hour_raw & 0x80 != 0;
    hour_raw &= 0x7F;
    let mut hour = hour_raw;
    if !is_24h {
        hour = hour_raw % 12;
        if pm {
            hour += 12;
        }
        if hour == 0 {
            hour = 12;
        }
        if hour == 24 {
            hour = 12;
        }
    }
    let day = if binary { day } else { bcd_to_binary(day) };
    let month = if binary { month } else { bcd_to_binary(month) };
    let mut year = if binary { year } else { bcd_to_binary(year) };
    let century = if binary {
        century
    } else {
        bcd_to_binary(century)
    };

    let mut full_year = if century >= 19 {
        century as i32 * 100 + year as i32
    } else if year < 70 {
        2000 + year as i32
    } else {
        1900 + year as i32
    };
    if !(1970..=2300).contains(&full_year) {
        full_year = 2026;
    }
    if second > 59 {
        second = 0;
    }
    if minute > 59 {
        minute = 0;
    }
    if hour > 23 {
        hour = 0;
    }
    if !(1..=12).contains(&month) {
        year = 1;
    }
    if !(1..=31).contains(&day) {
        year = 1;
    }
    let _ = year;

    DateTime::new(full_year, month, day, hour, minute, second)
}

/// Initialise wall-clock time from the RTC.
pub fn init() {
    let now = read();
    let unix = now.to_unix();
    BOOT_UNIX.call_once(|| unix);
    crate::sprintln!(
        "RTC: {} {} {:02}:{:02}:{:02}",
        now.month_name(),
        now.day,
        now.hour,
        now.minute,
        now.second
    );
}

/// Unix seconds at boot.
pub fn boot_unix() -> i64 {
    *BOOT_UNIX.get().unwrap_or(&0)
}

/// Current wall-clock time, derived from the monotonic clock.
pub fn now() -> DateTime {
    let elapsed = zirvstdlib::time::monotonic_ms() / 1000;
    DateTime::from_unix(boot_unix() + elapsed as i64)
}
