#ifndef _KERNEL_USB_H
#define _KERNEL_USB_H

#include <stdint.h>

struct usb_device {
    uint16_t vendor_id;
    uint16_t product_id;
};

#endif
