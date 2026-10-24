//! Minimal audio protocol and output boundary.
//!
//! Discovery is intentionally separate from playback. The HDA register and
//! stream descriptors below are the stable contract for the first virtual
//! audio backend; unsupported codecs remain reported as detected only.

use zirvstdlib::sync::Once;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct BufferDescriptor {
    pub address: u64,
    pub length: u32,
    pub flags: u32,
}

#[derive(Clone, Copy)]
pub struct SoundStatus {
    pub detected: bool,
    pub output_ready: bool,
    pub sample_rate: u32,
    pub channels: u8,
}

static STATUS: Once<SoundStatus> = Once::new();

pub fn init(detected: bool) {
    STATUS.call_once(|| SoundStatus {
        detected,
        output_ready: false,
        sample_rate: 48_000,
        channels: 2,
    });
    let status = status();
    crate::sprintln!(
        "SOUND: detected={} output={} format={}Hz/{}ch",
        status.detected,
        status.output_ready,
        status.sample_rate,
        status.channels
    );
}

pub fn status() -> SoundStatus {
    STATUS.get().copied().unwrap_or(SoundStatus {
        detected: false,
        output_ready: false,
        sample_rate: 48_000,
        channels: 2,
    })
}

pub fn output(_samples: &[i16]) -> Result<(), &'static str> {
    if !status().output_ready {
        return Err("audio output is detected but playback is not enabled");
    }
    Ok(())
}
