//! Kernel logging.
//!
//! Records go to the serial console and into a small in-memory ring so the
//! desktop's terminal can display `dmesg` output.

use core::fmt::Write;

use zirvstdlib::fmt::FixedString;
use zirvstdlib::log::{Level, Logger, Record};
use zirvstdlib::ring::RingBuffer;
use zirvstdlib::sync::SpinLock;

const LINE_CAPACITY: usize = 192;
const RING_CAPACITY: usize = 128;

#[derive(Clone, Copy)]
pub struct LogLine {
    pub level: Level,
    pub len: u8,
    pub text: [u8; LINE_CAPACITY],
}

impl LogLine {
    const fn empty() -> Self {
        Self {
            level: Level::Info,
            len: 0,
            text: [0; LINE_CAPACITY],
        }
    }

    pub fn as_str(&self) -> &str {
        core::str::from_utf8(&self.text[..self.len as usize]).unwrap_or("<invalid>")
    }
}

static LINES: SpinLock<RingBuffer<LogLine, RING_CAPACITY>> = SpinLock::new(RingBuffer::new());

struct SerialLogger;

impl Logger for SerialLogger {
    fn log(&self, record: &Record<'_>) {
        let seconds = record.timestamp_ms / 1000;
        let millis = record.timestamp_ms % 1000;
        let mut line = FixedString::<LINE_CAPACITY>::new();
        let _ = write!(
            line,
            "[{:>5}.{:03}] {:<5} {}: {}",
            seconds,
            millis,
            record.level.as_str(),
            record.module,
            record.message
        );
        crate::arch::serial::write_str(line.as_str());
        crate::arch::serial::write_str("\n");

        let mut entry = LogLine::empty();
        entry.level = record.level;
        let bytes = line.as_str().as_bytes();
        let count = bytes.len().min(LINE_CAPACITY);
        entry.text[..count].copy_from_slice(&bytes[..count]);
        entry.len = count as u8;
        let mut ring = LINES.lock();
        let _ = ring.push_overwrite(entry);
    }
}

static LOGGER: SerialLogger = SerialLogger;

pub fn init() {
    zirvstdlib::log::set_logger(&LOGGER);
}

/// Copy recent log lines into `output`, oldest first.  Returns the count.
pub fn recent(output: &mut [LogLine]) -> usize {
    let ring = LINES.lock();
    let skip = ring.len().saturating_sub(output.len());
    let mut count = 0;
    for line in ring.iter().skip(skip) {
        if count >= output.len() {
            break;
        }
        output[count] = *line;
        count += 1;
    }
    count
}

/// The most recent log line, if any.
pub fn last() -> Option<LogLine> {
    let ring = LINES.lock();
    ring.peek_back().copied()
}

#[macro_export]
macro_rules! kinfo {
    ($module:expr, $($arg:tt)*) => {
        ::zirvstdlib::log::info($module, format_args!($($arg)*))
    };
}

#[macro_export]
macro_rules! kwarn {
    ($module:expr, $($arg:tt)*) => {
        ::zirvstdlib::log::warn($module, format_args!($($arg)*))
    };
}

#[macro_export]
macro_rules! kerror {
    ($module:expr, $($arg:tt)*) => {
        ::zirvstdlib::log::error($module, format_args!($($arg)*))
    };
}
