//! Hardware drivers.

pub mod pci;
pub mod ps2;
pub mod rtc;

/// Initialise every driver that does not need the heap.
///
/// This runs before interrupts are enabled so that PS/2 command
/// acknowledgements can be polled reliably.
pub fn init_early() {
    pci::init();
    rtc::init();
    ps2::init();
}

/// Initialise drivers that need the heap or the final display geometry.
pub fn init_late(width: i32, height: i32) {
    ps2::set_screen(width, height);
}
