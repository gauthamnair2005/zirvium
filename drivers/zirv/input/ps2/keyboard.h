/* drivers/zirv/input/ps2/keyboard.h
 * Zirvium Kernel — PS/2 Keyboard driver
 *
 * Translates PS/2 scan-codes (set 2 via i8042 translation) into a simple
 * Zirvium key-event structure and places events in a ring buffer that
 * userspace can read from /zirv/input/keyboard0.
 */
#ifndef ZIRVIUM_DRIVERS_INPUT_PS2_KEYBOARD_H
#define ZIRVIUM_DRIVERS_INPUT_PS2_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

/* ── Key event ───────────────────────────────────────────────────────────── */
typedef struct {
    uint16_t keycode;   /* MOSIX virtual key code */
    bool     pressed;   /* true = key down, false = key up */
    uint8_t  mods;      /* modifier bitmask (see below) */
} key_event_t;

/* Modifier bits */
#define MOD_LSHIFT  (1 << 0)
#define MOD_RSHIFT  (1 << 1)
#define MOD_LCTRL   (1 << 2)
#define MOD_RCTRL   (1 << 3)
#define MOD_LALT    (1 << 4)
#define MOD_RALT    (1 << 5)
#define MOD_CAPS    (1 << 6)
#define MOD_NUMLOCK (1 << 7)

/* ── MOSIX virtual keycodes (partial; matches USB HID usage page 0x07) ───── */
#define KEY_NONE    0x00
#define KEY_A       0x04
#define KEY_B       0x05
#define KEY_C       0x06
#define KEY_Z       0x1D
#define KEY_1       0x1E
#define KEY_2       0x1F
#define KEY_0       0x27
#define KEY_ENTER   0x28
#define KEY_ESC     0x29
#define KEY_BSPACE  0x2A
#define KEY_TAB     0x2B
#define KEY_SPACE   0x2C
#define KEY_F1      0x3A
#define KEY_F12     0x45
#define KEY_UP      0x52
#define KEY_DOWN    0x51
#define KEY_LEFT    0x50
#define KEY_RIGHT   0x4F

/* ── Ring buffer size ────────────────────────────────────────────────────── */
#define KBD_RING_SIZE  64

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * keyboard_init - attach to i8042 port 1 and register with /zirv/input/keyboard0.
 */
void keyboard_init(void);

/**
 * keyboard_read_event - dequeue one key event.
 * Returns true if an event was available, false if the buffer was empty.
 */
bool keyboard_read_event(key_event_t *out);

/**
 * keyboard_pending - return the number of pending events.
 */
int keyboard_pending(void);

/**
 * keyboard_read_ascii - dequeue the next pressed key and convert to ASCII.
 * Returns the ASCII character, or 0 if no pressed key event is available.
 */
int keyboard_read_ascii(void);

#endif /* ZIRVIUM_DRIVERS_INPUT_PS2_KEYBOARD_H */
