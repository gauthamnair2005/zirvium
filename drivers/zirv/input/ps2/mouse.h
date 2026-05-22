#ifndef ZIRVIUM_DRIVERS_INPUT_PS2_MOUSE_H
#define ZIRVIUM_DRIVERS_INPUT_PS2_MOUSE_H

#include <stdint.h>

typedef struct {
    int32_t  dx;
    int32_t  dy;
    uint8_t  buttons;
} mouse_event_t;

/* Read one event from the mouse ring buffer (non-blocking).
 * Returns 0 on success, -1 if no event available. */
int mouse_read_event(mouse_event_t *ev);

#endif
