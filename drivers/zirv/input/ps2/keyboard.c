/* drivers/zirv/input/ps2/keyboard.c
 * Zirvium Kernel — PS/2 keyboard scan-code decoder
 *
 * The i8042 controller translates scan-code set 2 to set 1 by default
 * (XLAT bit in config).  We receive set-1 scan-codes and convert them
 * to MOSIX virtual keycodes.
 */
#include "keyboard.h"
#include "i8042.h"
#include "drivers/zirv/device.h"
#include "drivers/serial/serial.h"
#include "fs/mosix.h"
#include "drivers/compat/linux_compat.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Local keycodes used only in the scan-code table below ──────────────── */
#define KEY_LCTRL   0x01
#define KEY_LSHIFT  0x02
#define KEY_RSHIFT  0x03
#define KEY_LALT    0x39
#define KEY_CAPS    0x3A
#define KEY_NUMLOCK 0x53

/* ── Scan-code set 1 → MOSIX keycode table ───────────────────────────────── */
/* Index = scan-code byte (make code); value = MOSIX keycode */
static const uint8_t sc1_to_keycode[128] = {
    /* 0x00 */ KEY_NONE,  KEY_ESC,  KEY_1,    KEY_2,    /* 0 - 3 */
    /* 0x04 */ 0x23,      0x24,     0x25,     0x26,     /* 4 key, 5, 6, 7 */
    /* 0x08 */ 0x27+1,    0x27+2,   0x27+3,   0x27+4,   /* 8-B */
    /* 0x0C */ 0x27+5,    0x27+6,   KEY_BSPACE,KEY_TAB, /* C-F */
    /* 0x10 */ 0x14,      0x1A,     0x08,     0x15,     /* Q W E R */
    /* 0x14 */ 0x17,      0x1C,     0x18,     0x0C,     /* T Y U I */
    /* 0x18 */ 0x12,      0x13,     0x2F,     0x30,     /* O P [ ] */
    /* 0x1C */ KEY_ENTER, KEY_LCTRL,0x04,     0x16,     /* enter lctrl A S */
    /* 0x20 */ 0x07,      0x09,     0x0A,     0x0B,     /* D F G H */
    /* 0x24 */ 0x0D,      0x0E,     0x0F,     0x33,     /* J K L ; */
    /* 0x28 */ 0x34,      0x35,     KEY_LSHIFT,0x31,    /* ' ` lshift \ */
    /* 0x2C */ 0x1D,      0x1B,     0x06,     0x19,     /* Z X C V */
    /* 0x30 */ 0x05,      0x11,     0x10,     0x36,     /* B N M , */
    /* 0x34 */ 0x37,      0x38,     KEY_RSHIFT,0x55,    /* . / rshift */
    /* 0x38 */ KEY_LALT,  KEY_SPACE,KEY_CAPS, KEY_F1,   /* lalt space caps f1 */
    /* 0x3C */ KEY_F1+1,  KEY_F1+2, KEY_F1+3, KEY_F1+4, /* f2-f5 */
    /* 0x40 */ KEY_F1+5,  KEY_F1+6, KEY_F1+7, KEY_F1+8, /* f6-f9 */
    /* 0x44 */ KEY_F1+9,  KEY_NUMLOCK, 0x47, 0x48,      /* f10 numlock scroll */
    /* 0x48 */ KEY_UP,    0x4B,     0x4C,     KEY_RIGHT, /* up - kp5 right */
    /* 0x4C */ 0x4D,      KEY_DOWN, 0x4E,     0x4F,     /* + down pgdn end */
    /* 0x50 */ KEY_DOWN,  0x51,     0x52,     0x63,     /* del ins */
    /* 0x54 */ KEY_NONE,  KEY_NONE, KEY_NONE, KEY_F1+10, /* f11 */
    /* 0x58 */ KEY_F1+11, KEY_NONE, KEY_NONE, KEY_NONE,
    /* 0x5C */ KEY_NONE,  KEY_NONE, KEY_NONE, KEY_NONE,
    /* 0x60 */ KEY_NONE,  KEY_NONE, KEY_NONE, KEY_NONE,
    /* 0x64 */ KEY_NONE,  KEY_NONE, KEY_NONE, KEY_NONE,
    /* 0x68 */ KEY_NONE,  KEY_NONE, KEY_NONE, KEY_NONE,
    /* 0x6C */ KEY_NONE,  KEY_NONE, KEY_NONE, KEY_NONE,
    /* 0x70 */ KEY_NONE,  KEY_NONE, KEY_NONE, KEY_NONE,
    /* 0x74 */ KEY_NONE,  KEY_NONE, KEY_NONE, KEY_NONE,
    /* 0x78 */ KEY_NONE,  KEY_NONE, KEY_NONE, KEY_NONE,
    /* 0x7C */ KEY_NONE,  KEY_NONE, KEY_NONE, KEY_NONE,
};

/* ── Ring buffer ──────────────────────────────────────────────────────────── */
static key_event_t ring[KBD_RING_SIZE];
static int ring_head = 0, ring_tail = 0;

static void ring_push(key_event_t ev)
{
    int next = (ring_head + 1) % KBD_RING_SIZE;
    if (next != ring_tail) {
        ring[ring_head] = ev;
        ring_head = next;
    }
}

/* ── Modifier state ──────────────────────────────────────────────────────── */
static uint8_t mods = 0;

static void update_mods(uint16_t kc, bool pressed)
{
    uint8_t bit = 0;
    switch (kc) {
    case 0x02 /* LSHIFT */: bit = MOD_LSHIFT; break;
    case 0x03 /* RSHIFT */: bit = MOD_RSHIFT; break;
    case 0x01 /* LCTRL  */: bit = MOD_LCTRL;  break;
    case 0x39 /* LALT   */: bit = MOD_LALT;   break;
    }
    if (bit) {
        if (pressed) mods |= bit;
        else         mods &= ~bit;
    }
    if (kc == 0x3A /* CAPS */ && pressed)
        mods ^= MOD_CAPS;
}

/* ── i8042 callback ──────────────────────────────────────────────────────── */
static bool ext_key = false;  /* 0xE0 prefix seen */

static void kbd_byte_handler(uint8_t byte, void *data)
{
    (void)data;

    if (byte == 0xE0) { ext_key = true; return; }

    bool released = (byte & 0x80) != 0;
    uint8_t sc = byte & 0x7F;
    (void)ext_key;  /* extended key handling is future work */
    ext_key = false;

    uint16_t kc = (sc < 128) ? sc1_to_keycode[sc] : KEY_NONE;

    update_mods(kc, !released);

    key_event_t ev = { .keycode = kc, .pressed = !released, .mods = mods };
    ring_push(ev);
}

/* ── Public API ───────────────────────────────────────────────────────────── */
void keyboard_init(void)
{
    serial_puts(SERIAL_COM1, "[kbd] Initialising PS/2 keyboard\n");

    /* Reset keyboard */
    i8042_send_kbd(0xFF);
    /* Set scan-code set 1 */
    i8042_send_kbd(0xF0);
    i8042_send_kbd(0x01);

    /* Register byte handler with i8042 */
    i8042_register_port_handler(I8042_PORT_KBD, kbd_byte_handler, NULL);

    /* Register with the /zirv device registry */
    device_desc_t *desc = (device_desc_t *)kzalloc(sizeof(device_desc_t), 0);
    if (desc) {
        desc->bus_class   = DEV_CLASS_INPUT_KEYBOARD;
        desc->media_class = DEV_CLASS_INPUT_KEYBOARD;
        desc->present     = true;
        const char *model = "PS/2 Keyboard";
        for (int i = 0; model[i] && i < 63; i++) desc->model[i] = model[i];
        vfs_register_device(DEV_CLASS_INPUT_KEYBOARD,
                            DEV_CLASS_INPUT_KEYBOARD, 0, desc);
    }

    serial_puts(SERIAL_COM1, "[kbd] PS/2 keyboard ready\n");
}

bool keyboard_read_event(key_event_t *out)
{
    if (ring_tail == ring_head) return false;
    *out = ring[ring_tail];
    ring_tail = (ring_tail + 1) % KBD_RING_SIZE;
    return true;
}

int keyboard_pending(void)
{
    return (ring_head - ring_tail + KBD_RING_SIZE) % KBD_RING_SIZE;
}

int keyboard_read_ascii(void)
{
    key_event_t ev;
    while (keyboard_read_event(&ev)) {
        if (!ev.pressed) continue;

        int shift = (ev.mods & (MOD_LSHIFT | MOD_RSHIFT)) ? 1 : 0;
        int caps  = (ev.mods & MOD_CAPS) ? 1 : 0;

        /* Letters a-z (USB HID usage 0x04-0x1D) */
        if (ev.keycode >= 0x04 && ev.keycode <= 0x1D) {
            char c = 'a' + (int)(ev.keycode - 0x04);
            if (shift ^ caps)
                c -= 0x20;
            return c;
        }

        /* Digits 1-9 (USB HID usage 0x1E-0x26) */
        if (ev.keycode >= 0x1E && ev.keycode <= 0x26) {
            static const char shifted[] = "!@#$%^&*(";
            if (shift)
                return shifted[ev.keycode - 0x1E];
            return '1' + (int)(ev.keycode - 0x1E);
        }

        /* Digit 0 (USB HID usage 0x27) */
        if (ev.keycode == 0x27)
            return shift ? ')' : '0';

        /* Enter */
        if (ev.keycode == 0x28)
            return '\n';

        /* Backspace */
        if (ev.keycode == 0x2A)
            return '\b';

        /* Tab */
        if (ev.keycode == 0x2B)
            return '\t';

        /* Space */
        if (ev.keycode == 0x2C)
            return ' ';

        /* Punctuation keycodes (USB HID 0x2D-0x38, skipping 0x32) */
        switch (ev.keycode) {
        case 0x2D: return shift ? '_' : '-';
        case 0x2E: return shift ? '+' : '=';
        case 0x2F: return shift ? '{' : '[';
        case 0x30: return shift ? '}' : ']';
        case 0x31: return shift ? '|' : '\\';
        case 0x33: return shift ? ':' : ';';
        case 0x34: return shift ? '"' : '\'';
        case 0x35: return shift ? '~' : '`';
        case 0x36: return shift ? '<' : ',';
        case 0x37: return shift ? '>' : '.';
        case 0x38: return shift ? '?' : '/';
        }
    }
    return 0;
}
