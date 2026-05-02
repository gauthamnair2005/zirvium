/* drivers/zirv/usb_storage.c
 * Zirvium Kernel — USB Mass Storage stub
 *
 * USB enumeration is handled by a dedicated XHCI/EHCI host controller driver.
 * Once a USB mass storage device is enumerated, the HC driver calls
 * zirv_register_device() to create the appropriate /zirv/usb/<type><n> node.
 *
 * This file provides the init stub that will kick off USB HC detection.
 */
#include "usb_storage.h"
#include "device.h"

void usb_storage_init(void)
{
    /*
     * TODO: scan PCI for XHCI (class 0x0C, subclass 0x03, progif 0x30) and
     * EHCI (progif 0x20) host controllers.  For each detected controller:
     *   1. Initialise the HC (reset, set up ring buffers)
     *   2. Enumerate root-hub ports
     *   3. For each connected USB mass storage device, call
     *      zirv_register_device(DEV_CLASS_USB, <media>, model, &usb_msd_ops)
     *      which creates /zirv/usb/pendrive0, /zirv/usb/hdd0, etc.
     *
     * Until the HC driver is implemented, no USB devices will appear in /zirv.
     */
}
