//! PS/2 keyboard and mouse.
//!
//! The controller is initialised before interrupts are enabled so that the
//! command acknowledgements can be polled safely.  Afterwards IRQ1 and IRQ12
//! feed the kernel input queue.

use core::sync::atomic::{AtomicBool, AtomicI32, AtomicU8, AtomicU16, Ordering};

use zirvstdlib::abi::{InputEvent, InputKind, buttons, key, modifiers};
use zirvstdlib::sync::SpinLock;

use crate::arch::port::{inb, outb};

const DATA_PORT: u16 = 0x60;
const STATUS_PORT: u16 = 0x64;
const COMMAND_PORT: u16 = 0x64;

const STATUS_OUTPUT_FULL: u8 = 0x01;
const STATUS_INPUT_FULL: u8 = 0x02;

static SHIFT: AtomicBool = AtomicBool::new(false);
static CTRL: AtomicBool = AtomicBool::new(false);
static ALT: AtomicBool = AtomicBool::new(false);
static SUPER: AtomicBool = AtomicBool::new(false);
static CAPS: AtomicBool = AtomicBool::new(false);
static NUM_LOCK: AtomicBool = AtomicBool::new(false);
static EXTENDED: AtomicBool = AtomicBool::new(false);
static SKIP_E1: AtomicU8 = AtomicU8::new(0);

static CURSOR_X: AtomicI32 = AtomicI32::new(0);
static CURSOR_Y: AtomicI32 = AtomicI32::new(0);
static SCREEN_W: AtomicI32 = AtomicI32::new(1280);
static SCREEN_H: AtomicI32 = AtomicI32::new(800);
static MOUSE_BUTTONS: AtomicU16 = AtomicU16::new(0);
static PACKET_SIZE: AtomicU8 = AtomicU8::new(3);

struct MouseDecoder {
    packet: [u8; 4],
    index: usize,
}

static MOUSE: SpinLock<MouseDecoder> = SpinLock::new(MouseDecoder {
    packet: [0; 4],
    index: 0,
});

fn modifiers() -> u16 {
    let mut value = 0;
    if SHIFT.load(Ordering::Relaxed) {
        value |= modifiers::SHIFT;
    }
    if CTRL.load(Ordering::Relaxed) {
        value |= modifiers::CTRL;
    }
    if ALT.load(Ordering::Relaxed) {
        value |= modifiers::ALT;
    }
    if SUPER.load(Ordering::Relaxed) {
        value |= modifiers::SUPER;
    }
    if CAPS.load(Ordering::Relaxed) {
        value |= modifiers::CAPS_LOCK;
    }
    if NUM_LOCK.load(Ordering::Relaxed) {
        value |= modifiers::NUM_LOCK;
    }
    value
}

// ---------------------------------------------------------------------------
// Controller helpers
// ---------------------------------------------------------------------------

unsafe fn wait_input_clear() -> bool {
    for _ in 0..100_000 {
        // SAFETY: the PS/2 status port is always readable.
        if unsafe { inb(STATUS_PORT) } & STATUS_INPUT_FULL == 0 {
            return true;
        }
        core::hint::spin_loop();
    }
    false
}

unsafe fn wait_output_full() -> bool {
    for _ in 0..100_000 {
        // SAFETY: the PS/2 status port is always readable.
        if unsafe { inb(STATUS_PORT) } & STATUS_OUTPUT_FULL != 0 {
            return true;
        }
        core::hint::spin_loop();
    }
    false
}

unsafe fn write_command(command: u8) {
    // SAFETY: the caller runs during single-threaded bring-up.
    unsafe {
        if wait_input_clear() {
            outb(COMMAND_PORT, command);
        }
    }
}

unsafe fn write_data(value: u8) {
    // SAFETY: the caller runs during single-threaded bring-up.
    unsafe {
        if wait_input_clear() {
            outb(DATA_PORT, value);
        }
    }
}

unsafe fn read_data() -> Option<u8> {
    // SAFETY: the caller runs during single-threaded bring-up.
    unsafe {
        if wait_output_full() {
            Some(inb(DATA_PORT))
        } else {
            None
        }
    }
}

unsafe fn flush_output() {
    for _ in 0..32 {
        // SAFETY: reading the status register is always safe.
        if unsafe { inb(STATUS_PORT) } & STATUS_OUTPUT_FULL == 0 {
            break;
        }
        // SAFETY: drain stale bytes.
        unsafe {
            let _ = inb(DATA_PORT);
        }
    }
}

unsafe fn mouse_command(command: u8) {
    // SAFETY: single-threaded initialisation.
    unsafe {
        write_command(0xD4);
        write_data(command);
        let _ = read_data();
    }
}

/// Initialise both PS/2 ports.  Interrupts must still be disabled.
pub fn init() {
    // SAFETY: this runs once during boot with interrupts disabled.
    unsafe {
        flush_output();
        write_command(0xAD); // Disable keyboard.
        write_command(0xA7); // Disable auxiliary device.

        let mut config = 0u8;
        write_command(0x20); // Read configuration byte.
        if let Some(value) = read_data() {
            config = value;
        }
        config |= 0x01; // IRQ1
        config |= 0x02; // IRQ12
        config |= 0x40; // Translate scancode set 2 to set 1.
        config &= !0x30; // Both clocks enabled.
        write_command(0x60);
        write_data(config);

        write_command(0xAE); // Enable keyboard.
        write_command(0xA8); // Enable auxiliary device.

        // Keyboard: enable scanning.
        write_data(0xF4);
        let _ = read_data();

        // Mouse: reset, then try to enable the IntelliMouse wheel protocol.
        mouse_command(0xF6);
        mouse_command(0xF3);
        mouse_command(200);
        mouse_command(0xF3);
        mouse_command(100);
        mouse_command(0xF3);
        mouse_command(80);
        mouse_command(0xF2);
        let id = read_data().unwrap_or(0);
        PACKET_SIZE.store(if id == 3 || id == 4 { 4 } else { 3 }, Ordering::Relaxed);
        mouse_command(0xF4);

        flush_output();
    }
    crate::sprintln!(
        "PS/2: keyboard and {}-byte mouse ready",
        PACKET_SIZE.load(Ordering::Relaxed)
    );
}

/// Tell the mouse driver the desktop resolution.
pub fn set_screen(width: i32, height: i32) {
    SCREEN_W.store(width.max(1), Ordering::Relaxed);
    SCREEN_H.store(height.max(1), Ordering::Relaxed);
    CURSOR_X.store(width / 2, Ordering::Relaxed);
    CURSOR_Y.store(height / 2, Ordering::Relaxed);
}

pub fn cursor_position() -> (i32, i32) {
    (
        CURSOR_X.load(Ordering::Relaxed),
        CURSOR_Y.load(Ordering::Relaxed),
    )
}

pub fn set_cursor_position(x: i32, y: i32) {
    CURSOR_X.store(x, Ordering::Relaxed);
    CURSOR_Y.store(y, Ordering::Relaxed);
}

pub fn mouse_buttons() -> u16 {
    MOUSE_BUTTONS.load(Ordering::Relaxed)
}

fn clamp_cursor() {
    let width = SCREEN_W.load(Ordering::Relaxed);
    let height = SCREEN_H.load(Ordering::Relaxed);
    let x = CURSOR_X.load(Ordering::Relaxed).clamp(0, width - 1);
    let y = CURSOR_Y.load(Ordering::Relaxed).clamp(0, height - 1);
    CURSOR_X.store(x, Ordering::Relaxed);
    CURSOR_Y.store(y, Ordering::Relaxed);
}

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

/// Translate a set-1 scancode to a printable or special key code.
fn scancode_key(code: u8, shifted: bool) -> u32 {
    // Letters.
    let letter = match code {
        0x10 => Some('q'),
        0x11 => Some('w'),
        0x12 => Some('e'),
        0x13 => Some('r'),
        0x14 => Some('t'),
        0x15 => Some('y'),
        0x16 => Some('u'),
        0x17 => Some('i'),
        0x18 => Some('o'),
        0x19 => Some('p'),
        0x1E => Some('a'),
        0x1F => Some('s'),
        0x20 => Some('d'),
        0x21 => Some('f'),
        0x22 => Some('g'),
        0x23 => Some('h'),
        0x24 => Some('j'),
        0x25 => Some('k'),
        0x26 => Some('l'),
        0x2C => Some('z'),
        0x2D => Some('x'),
        0x2E => Some('c'),
        0x2F => Some('v'),
        0x30 => Some('b'),
        0x31 => Some('n'),
        0x32 => Some('m'),
        _ => None,
    };
    if let Some(letter) = letter {
        // Caps lock applies to letters only; shift inverts it.
        let upper = CAPS.load(Ordering::Relaxed) ^ shifted;
        return if upper {
            (letter as u8).to_ascii_uppercase() as u32
        } else {
            letter as u32
        };
    }

    // Digit row and punctuation.
    let character = match code {
        0x02 => {
            if shifted {
                '!'
            } else {
                '1'
            }
        }
        0x03 => {
            if shifted {
                '@'
            } else {
                '2'
            }
        }
        0x04 => {
            if shifted {
                '#'
            } else {
                '3'
            }
        }
        0x05 => {
            if shifted {
                '$'
            } else {
                '4'
            }
        }
        0x06 => {
            if shifted {
                '%'
            } else {
                '5'
            }
        }
        0x07 => {
            if shifted {
                '^'
            } else {
                '6'
            }
        }
        0x08 => {
            if shifted {
                '&'
            } else {
                '7'
            }
        }
        0x09 => {
            if shifted {
                '*'
            } else {
                '8'
            }
        }
        0x0A => {
            if shifted {
                '('
            } else {
                '9'
            }
        }
        0x0B => {
            if shifted {
                ')'
            } else {
                '0'
            }
        }
        0x0C => {
            if shifted {
                '_'
            } else {
                '-'
            }
        }
        0x0D => {
            if shifted {
                '+'
            } else {
                '='
            }
        }
        0x1A => {
            if shifted {
                '{'
            } else {
                '['
            }
        }
        0x1B => {
            if shifted {
                '}'
            } else {
                ']'
            }
        }
        0x27 => {
            if shifted {
                ':'
            } else {
                ';'
            }
        }
        0x28 => {
            if shifted {
                '"'
            } else {
                '\''
            }
        }
        0x29 => {
            if shifted {
                '~'
            } else {
                '`'
            }
        }
        0x2B => {
            if shifted {
                '|'
            } else {
                '\\'
            }
        }
        0x33 => {
            if shifted {
                '<'
            } else {
                ','
            }
        }
        0x34 => {
            if shifted {
                '>'
            } else {
                '.'
            }
        }
        0x35 => {
            if shifted {
                '?'
            } else {
                '/'
            }
        }
        0x39 => ' ',
        _ => '\0',
    };
    if character != '\0' {
        return character as u32;
    }

    // Special keys.
    match code {
        0x01 => key::ESCAPE,
        0x0E => key::BACKSPACE,
        0x0F => key::TAB,
        0x1C => key::ENTER,
        0x1D => key::NONE,
        0x2A | 0x36 => key::NONE,
        0x3A => key::CAPS_LOCK,
        0x3B => key::F1,
        0x3C => key::F2,
        0x3D => key::F3,
        0x3E => key::F4,
        0x3F => key::F5,
        0x40 => key::F6,
        0x41 => key::F7,
        0x42 => key::F8,
        0x43 => key::F9,
        0x44 => key::F10,
        0x45 => key::NUM_LOCK,
        0x46 => key::SCROLL_LOCK,
        0x57 => key::F11,
        0x58 => key::F12,
        _ => key::NONE,
    }
}

fn extended_key(code: u8) -> u32 {
    match code {
        0x1C => key::ENTER,
        0x1D => key::NONE,
        0x35 => b'/' as u32,
        0x38 => key::NONE,
        0x47 => key::HOME,
        0x48 => key::UP,
        0x49 => key::PAGE_UP,
        0x4B => key::LEFT,
        0x4D => key::RIGHT,
        0x4F => key::END,
        0x50 => key::DOWN,
        0x51 => key::PAGE_DOWN,
        0x52 => key::INSERT,
        0x53 => key::DELETE,
        0x5B => key::SUPER,
        0x5C => key::SUPER,
        0x5D => key::MENU,
        _ => key::NONE,
    }
}

fn update_modifier(code: u8, pressed: bool, extended: bool) {
    match code {
        0x2A | 0x36 => SHIFT.store(pressed, Ordering::Relaxed),
        0x1D => CTRL.store(pressed, Ordering::Relaxed),
        0x38 => ALT.store(pressed, Ordering::Relaxed),
        0x3A if pressed => {
            let value = CAPS.load(Ordering::Relaxed);
            CAPS.store(!value, Ordering::Relaxed);
        }
        0x45 if pressed => {
            let value = NUM_LOCK.load(Ordering::Relaxed);
            NUM_LOCK.store(!value, Ordering::Relaxed);
        }
        0x5B | 0x5C if extended => SUPER.store(pressed, Ordering::Relaxed),
        _ => {}
    }
}

/// Called from IRQ1.
pub fn handle_keyboard() {
    // SAFETY: IRQ1 means the controller has a byte for us.
    let status = unsafe { inb(STATUS_PORT) };
    if status & STATUS_OUTPUT_FULL == 0 {
        return;
    }
    // SAFETY: reading the data port consumes the byte.
    let mut code = unsafe { inb(DATA_PORT) };

    if code == 0xE0 {
        EXTENDED.store(true, Ordering::Relaxed);
        return;
    }
    if code == 0xE1 {
        SKIP_E1.store(2, Ordering::Relaxed);
        return;
    }
    if SKIP_E1.load(Ordering::Relaxed) > 0 {
        SKIP_E1.fetch_sub(1, Ordering::Relaxed);
        return;
    }

    let extended = EXTENDED.swap(false, Ordering::Relaxed);
    let pressed = code & 0x80 == 0;
    code &= 0x7F;
    if code == 0 || code == 0xFA || code == 0xFE || code == 0xFF {
        return;
    }

    update_modifier(code, pressed, extended);
    let key_code = if extended {
        extended_key(code)
    } else {
        scancode_key(code, SHIFT.load(Ordering::Relaxed))
    };
    if key_code == key::NONE {
        return;
    }

    let event = InputEvent::key(
        if pressed {
            InputKind::KeyDown
        } else {
            InputKind::KeyUp
        },
        key_code,
        modifiers(),
    );
    crate::input::push(event);
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

fn button_bits(flags: u8) -> u16 {
    let mut value = 0;
    if flags & 0x01 != 0 {
        value |= buttons::LEFT;
    }
    if flags & 0x02 != 0 {
        value |= buttons::RIGHT;
    }
    if flags & 0x04 != 0 {
        value |= buttons::MIDDLE;
    }
    value
}

fn emit_pointer(kind: InputKind, x: i32, y: i32, value: i32) {
    let mut event = InputEvent::pointer(kind, x, y, MOUSE_BUTTONS.load(Ordering::Relaxed));
    event.value = value;
    crate::input::push(event);
}

/// Called from IRQ12.
pub fn handle_mouse() {
    // SAFETY: IRQ12 means the controller has a byte for us.
    let status = unsafe { inb(STATUS_PORT) };
    if status & STATUS_OUTPUT_FULL == 0 {
        return;
    }
    // SAFETY: reading the data port consumes the byte.
    let byte = unsafe { inb(DATA_PORT) };

    let mut decoder = MOUSE.lock();
    // Resynchronise if the first byte does not carry the "always one" bit.
    if decoder.index == 0 && byte & 0x08 == 0 {
        return;
    }
    let index = decoder.index;
    decoder.packet[index] = byte;
    decoder.index += 1;
    let wanted = PACKET_SIZE.load(Ordering::Relaxed) as usize;
    if decoder.index < wanted {
        return;
    }
    decoder.index = 0;
    let packet = decoder.packet;
    drop(decoder);

    let flags = packet[0];
    if flags & 0xC0 != 0 {
        // Overflow: drop the packet.
        return;
    }

    let mut dx = packet[1] as i32;
    let mut dy = packet[2] as i32;
    if flags & 0x10 != 0 {
        dx -= 256;
    }
    if flags & 0x20 != 0 {
        dy -= 256;
    }

    let old_buttons = MOUSE_BUTTONS.load(Ordering::Relaxed);
    let new_buttons = button_bits(flags);
    MOUSE_BUTTONS.store(new_buttons, Ordering::Relaxed);

    let (old_x, old_y) = cursor_position();
    if dx != 0 || dy != 0 {
        // PS/2 Y grows upwards; screen Y grows downwards.
        let x = old_x + dx;
        let y = old_y - dy;
        CURSOR_X.store(x, Ordering::Relaxed);
        CURSOR_Y.store(y, Ordering::Relaxed);
        clamp_cursor();
        let (x, y) = cursor_position();
        emit_pointer(InputKind::PointerMove, x, y, 0);
    }
    if old_buttons != new_buttons {
        let (x, y) = cursor_position();
        let kind = if new_buttons & buttons::LEFT != 0 {
            InputKind::PointerDown
        } else {
            InputKind::PointerUp
        };
        emit_pointer(kind, x, y, 0);
    }

    if wanted == 4 {
        let wheel = packet[3] as i8 as i32;
        if wheel != 0 {
            let (x, y) = cursor_position();
            emit_pointer(InputKind::Scroll, x, y, wheel);
        }
    }
}
