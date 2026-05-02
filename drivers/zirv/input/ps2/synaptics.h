/* drivers/zirv/input/ps2/synaptics.h
 * Zirvium Kernel — Synaptics TouchPad driver (PS/2 port 2 / i8042 aux)
 *
 * Most Intel laptop touchpads present themselves as a Synaptics PS/2
 * device on the i8042 auxiliary port (IRQ 12).
 *
 * Detection sequence:
 *  1. Send "Identify Synaptics" magic:  E8 00 E8 00 E8 00 E8 00 E9
 *  2. Parse 3-byte identify response;   byte[1] = 0x47 → Synaptics
 *  3. Enter absolute mode (E8 C8 E8 00 E8 28 E8 C8 F4)
 *  4. Receive 6-byte absolute packets and decode finger position
 *
 * Absolute coordinates are published to /zirv/input/touchpad0.
 */
#ifndef ZIRVIUM_DRIVERS_INPUT_PS2_SYNAPTICS_H
#define ZIRVIUM_DRIVERS_INPUT_PS2_SYNAPTICS_H

#include <stdint.h>
#include <stdbool.h>

/* ── Touchpad event ──────────────────────────────────────────────────────── */
typedef struct {
    int32_t  x, y;          /* absolute position (Synaptics units) */
    uint8_t  pressure;      /* 0 = no contact, 255 = hard press */
    uint8_t  fingers;       /* number of detected fingers (0, 1, 2) */
    bool     btn_left;
    bool     btn_right;
    bool     btn_middle;
} touchpad_event_t;

/* ── Ring buffer size ────────────────────────────────────────────────────── */
#define TP_RING_SIZE  32

/* ── Public API ───────────────────────────────────────────────────────────── */

/** synaptics_init - detect and initialise the Synaptics touchpad.
 *  Returns true if a Synaptics device was found on the aux port. */
bool synaptics_init(void);

/** synaptics_read_event - dequeue one touchpad event.
 *  Returns true if an event was available. */
bool synaptics_read_event(touchpad_event_t *out);

/** synaptics_pending - return the number of pending events. */
int synaptics_pending(void);

#endif /* ZIRVIUM_DRIVERS_INPUT_PS2_SYNAPTICS_H */
