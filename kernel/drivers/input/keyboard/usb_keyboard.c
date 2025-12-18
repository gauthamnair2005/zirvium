/*
 * USB Keyboard Driver
 * Zirvium OS
 */

#include <kernel/types.h>
#include <kernel/kernel.h>

#define USB_CLASS_HID 0x03
#define USB_PROTOCOL_KEYBOARD 0x01

typedef struct {
    uint8_t modifiers;
    uint8_t keys[6];
    uint8_t leds;
} usb_keyboard_t;

static usb_keyboard_t usb_kbd;

int usb_keyboard_init(void) {
    kprintf("USB-KBD: USB Keyboard driver\n");
    kprintf("USB-KBD: HID Class: 0x%x, Protocol: 0x%x\n", 
            USB_CLASS_HID, USB_PROTOCOL_KEYBOARD);
    return 0;
}

int usb_keyboard_read_key(void) {
    return 0;
}

void usb_keyboard_set_leds(uint8_t leds) {
    usb_kbd.leds = leds;
    kprintf("USB-KBD: LEDs set to 0x%x\n", leds);
}

void usb_keyboard_cleanup(void) {
    kprintf("USB-KBD: Driver unloaded\n");
}
