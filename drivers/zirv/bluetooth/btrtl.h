/* drivers/zirv/bluetooth/btrtl.h
 * Zirvium Kernel — Realtek Bluetooth driver (RTL8723DE BT 4.0 component)
 *
 * The RTL8723DE includes a Bluetooth 4.0 controller.  On most laptops it
 * is exposed as a USB device on an internal USB hub (not PCIe), or as a
 * UART-attached HCI device.  This driver covers both configurations.
 *
 * Linux reference: drivers/bluetooth/btrtl.c  (GPL-2.0)
 * Ported to Zirvium MOSIX APIs.
 *
 * HCI (Host Controller Interface) commands follow the Bluetooth Core Spec
 * Vol 4, Part E.
 */
#ifndef ZIRVIUM_DRIVERS_BLUETOOTH_BTRTL_H
#define ZIRVIUM_DRIVERS_BLUETOOTH_BTRTL_H

#include "drivers/compat/linux_compat.h"
#include <stdint.h>
#include <stdbool.h>

/* ── USB vendor/device IDs for RTL8723DE Bluetooth ──────────────────────── */
#define BTRTL_USB_VENDOR   0x0BDA   /* Realtek USB BT dongle VID */
#define BTRTL_USB_DEVICE   0xB009   /* RTL8723DE BT USB device   */

/* ── HCI packet types ────────────────────────────────────────────────────── */
#define HCI_CMD_PKT    0x01
#define HCI_ACL_PKT    0x02
#define HCI_SCO_PKT    0x03
#define HCI_EVENT_PKT  0x04

/* ── Common HCI commands ─────────────────────────────────────────────────── */
#define HCI_RESET                0x0C03
#define HCI_READ_LOCAL_VERSION   0x1001
#define HCI_READ_BD_ADDR         0x1009

/* ── Realtek vendor commands ─────────────────────────────────────────────── */
#define BTRTL_VENDOR_LOAD_FW     0xFC20
#define BTRTL_VENDOR_GET_ROM_VER 0xFC6D

/* ── Bluetooth device state ──────────────────────────────────────────────── */
typedef struct {
    uint8_t  bd_addr[6];   /* Bluetooth device address */
    uint16_t hci_rev;
    uint8_t  lmp_ver;
    uint16_t lmp_subver;
    bool     fw_loaded;
    bool     ready;
} btrtl_priv_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * btrtl_init - detect and initialise the RTL8723DE Bluetooth controller.
 * Registers the device as /zirv/net/bt0.
 *
 * @uart_port: UART I/O base if the BT component is UART-attached, or 0 to
 *             probe via USB.  On most RTL8723DE laptops pass 0.
 */
void btrtl_init(uint16_t uart_port);

/**
 * btrtl_get_bd_addr - copy the 6-byte Bluetooth device address into @buf.
 */
bool btrtl_get_bd_addr(uint8_t buf[6]);

#endif /* ZIRVIUM_DRIVERS_BLUETOOTH_BTRTL_H */
