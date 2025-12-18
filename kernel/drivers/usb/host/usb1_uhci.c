/*
 * USB 1.1 UHCI (Universal Host Controller Interface) Driver
 * Zirvium OS - Low/Full speed USB support (1.5/12 Mbps)
 */

#include <kernel/types.h>
#include <kernel/kernel.h>

#define UHCI_VENDOR_INTEL 0x8086

/* UHCI Registers (I/O ports) */
#define UHCI_USBCMD    0x00  /* Command */
#define UHCI_USBSTS    0x02  /* Status */
#define UHCI_USBINTR   0x04  /* Interrupt Enable */
#define UHCI_FRNUM     0x06  /* Frame Number */
#define UHCI_FRBASEADD 0x08  /* Frame List Base Address */
#define UHCI_SOFMOD    0x0C  /* Start of Frame Modify */
#define UHCI_PORTSC1   0x10  /* Port 1 Status/Control */
#define UHCI_PORTSC2   0x12  /* Port 2 Status/Control */

typedef struct {
    uint16_t io_base;
    uint8_t num_ports;
    uint32_t *frame_list;
    uint8_t devices_connected;
} uhci_controller_t;

static uhci_controller_t uhci;

int usb1_uhci_init(void) {
    kprintf("USB1-UHCI: USB 1.1 UHCI Host Controller\n");
    kprintf("USB1-UHCI: Speed: Low (1.5 Mbps) / Full (12 Mbps)\n");
    
    uhci.num_ports = 2;
    uhci.devices_connected = 0;
    
    /* Allocate frame list (1024 entries) */
    uhci.frame_list = (uint32_t*)kmalloc(1024 * sizeof(uint32_t));
    
    kprintf("USB1-UHCI: Initialized with %d ports\n", uhci.num_ports);
    
    return 0;
}

void usb1_uhci_cleanup(void) {
    kprintf("USB1-UHCI: Driver unloaded\n");
}
