/* drivers/zirv/usb_storage.h
 * Zirvium Kernel — USB Mass Storage device registration
 */
#ifndef ZIRVIUM_DRIVERS_ZIRV_USB_STORAGE_H
#define ZIRVIUM_DRIVERS_ZIRV_USB_STORAGE_H

/**
 * usb_storage_init - probe for USB mass storage devices (pen drives, HDDs,
 *                    SSDs) and register them under /zirv/usb/<type><n>.
 *
 * Requires the USB host controller driver to be initialised first.
 * Currently a stub — full XHCI/EHCI enumeration is a future task.
 */
void usb_storage_init(void);

#endif /* ZIRVIUM_DRIVERS_ZIRV_USB_STORAGE_H */
