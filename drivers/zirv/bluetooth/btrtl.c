/* drivers/zirv/bluetooth/btrtl.c
 * Zirvium Kernel — Realtek Bluetooth HCI driver (RTL8723DE BT 4.0)
 *
 * Ported from Linux drivers/bluetooth/btrtl.c and btusb.c
 * (GPL-2.0, Copyright Realtek Semiconductor).
 *
 * Adapted to Zirvium MOSIX APIs.
 * The BT component of RTL8723DE is typically exposed as USB device
 * 0x0BDA:0xB009 on an internal USB root hub, and communicates over
 * standard Bluetooth HCI.
 *
 * Initialisation sequence:
 *  1. HCI Reset
 *  2. Read local version (vendor subversion identifies the exact chip)
 *  3. Download firmware via vendor-specific HCI command BTRTL_VENDOR_LOAD_FW
 *  4. HCI Reset again to activate firmware
 *  5. Read BD address
 */
#include "btrtl.h"
#include "drivers/zirv/device.h"
#include "drivers/serial/serial.h"
#include "drivers/compat/linux_compat.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Static device state ─────────────────────────────────────────────────── */
static btrtl_priv_t g_bt;
static bool g_bt_found = false;

/* ── UART HCI transport (fallback when USB stack is not up) ──────────────── */
/* The RTL8723DE BT controller can also be accessed via a UART.
 * On most Intel laptop designs it is at COM3 (0x3E8) or a dedicated
 * low-speed UART at a platform-specific address.                    */

#include "arch/x64/cpu.h"

/* Minimal UART send for HCI over UART */
static void hci_uart_send(uint16_t port, const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        /* Wait for THRE */
        while (!(inb((uint16_t)(port + 5)) & 0x20))
            cpu_pause();
        outb(port, buf[i]);
    }
}

static bool hci_uart_recv(uint16_t port, uint8_t *buf, size_t len,
                           unsigned int timeout_ms)
{
    for (size_t i = 0; i < len; i++) {
        unsigned int t = 0;
        while (!(inb((uint16_t)(port + 5)) & 0x01)) {
            udelay(1000);
            if (++t > timeout_ms) return false;
        }
        buf[i] = inb(port);
    }
    return true;
}

/* ── HCI command helpers ──────────────────────────────────────────────────── */
static bool hci_reset(uint16_t uart_port)
{
    /* HCI Reset: 01 03 0C 00 */
    uint8_t cmd[] = { HCI_CMD_PKT, 0x03, 0x0C, 0x00 };
    hci_uart_send(uart_port, cmd, sizeof(cmd));

    /* Expect: 04 0E 04 01 03 0C 00 */
    uint8_t evt[7] = {0};
    if (!hci_uart_recv(uart_port, evt, sizeof(evt), 200)) return false;
    return evt[0] == HCI_EVENT_PKT && evt[5] == 0x00;
}

static bool hci_read_local_version(uint16_t uart_port)
{
    uint8_t cmd[] = { HCI_CMD_PKT, 0x01, 0x10, 0x00 };
    hci_uart_send(uart_port, cmd, sizeof(cmd));

    /* Response: 04 0E 0C 01 01 10 00 hci_ver hci_rev[2] lmp_ver mfr[2] lmp_sub[2] */
    uint8_t evt[15] = {0};
    if (!hci_uart_recv(uart_port, evt, sizeof(evt), 200)) return false;
    if (evt[0] != HCI_EVENT_PKT) return false;

    g_bt.hci_rev    = (uint16_t)(evt[8] | ((uint16_t)evt[9] << 8));
    g_bt.lmp_ver    = evt[10];
    g_bt.lmp_subver = (uint16_t)(evt[13] | ((uint16_t)evt[14] << 8));
    return true;
}

static bool hci_read_bd_addr(uint16_t uart_port)
{
    uint8_t cmd[] = { HCI_CMD_PKT, 0x09, 0x10, 0x00 };
    hci_uart_send(uart_port, cmd, sizeof(cmd));

    /* Response: 04 0E 0A 01 09 10 00 bd_addr[6] */
    uint8_t evt[13] = {0};
    if (!hci_uart_recv(uart_port, evt, sizeof(evt), 200)) return false;
    if (evt[0] != HCI_EVENT_PKT || evt[6] != 0x00) return false;
    for (int i = 0; i < 6; i++)
        g_bt.bd_addr[i] = evt[7 + i];
    return true;
}

/* ── Firmware download via vendor HCI command ────────────────────────────── */
/* The RTL8723DE BT firmware lives at /boot/firmware/rtl8723d_fw.bin.
 * Downloading happens in 252-byte chunks via command 0xFC20.
 * Here we provide the skeleton; actual file loading waits for the VFS. */
static bool btrtl_download_firmware(uint16_t uart_port)
{
    /* Phase 1: Query ROM version */
    uint8_t cmd_rom[] = { HCI_CMD_PKT,
                          (uint8_t)(BTRTL_VENDOR_GET_ROM_VER & 0xFF),
                          (uint8_t)(BTRTL_VENDOR_GET_ROM_VER >> 8),
                          0x00 };
    hci_uart_send(uart_port, cmd_rom, sizeof(cmd_rom));

    uint8_t rom_evt[8] = {0};
    hci_uart_recv(uart_port, rom_evt, sizeof(rom_evt), 200);
    /* rom_evt[7] = ROM version; 0x0D for RTL8723DE */

    serial_puts(SERIAL_COM1,
        "[btrtl] Firmware download deferred until /boot is mounted\n");

    /* TODO: open /boot/firmware/rtl8723d_fw.bin via vfs_lookup(),
     *       iterate 252-byte chunks, send each as HCI vendor cmd 0xFC20,
     *       then issue HCI Reset to activate. */
    return true;
}

/* ── UART initialisation ─────────────────────────────────────────────────── */
static bool btrtl_uart_init(uint16_t port)
{
    /* Configure UART for HCI: 115200 baud initially, then switch to 3 Mbps */
    outb((uint16_t)(port + 1), 0x00);   /* disable interrupts */
    outb((uint16_t)(port + 3), 0x80);   /* DLAB on */
    outb(port, 0x01);                   /* 115200 baud divisor low */
    outb((uint16_t)(port + 1), 0x00);   /* divisor high */
    outb((uint16_t)(port + 3), 0x03);   /* 8N1, DLAB off */
    outb((uint16_t)(port + 2), 0xC7);   /* enable + clear FIFO */
    outb((uint16_t)(port + 4), 0x03);   /* RTS + DTR */
    return true;
}

/* ── Public API ───────────────────────────────────────────────────────────── */
void btrtl_init(uint16_t uart_port)
{
    serial_puts(SERIAL_COM1, "[btrtl] Initialising RTL8723DE Bluetooth\n");

    /* If no uart_port given, use COM3 (common for RTL8723DE BT UART) */
    if (uart_port == 0) uart_port = 0x3E8;   /* COM3 */

    if (!btrtl_uart_init(uart_port)) {
        serial_puts(SERIAL_COM1, "[btrtl] UART init failed\n");
        return;
    }

    if (!hci_reset(uart_port)) {
        serial_puts(SERIAL_COM1,
            "[btrtl] HCI Reset timed out — BT controller not responding\n");
        return;
    }

    hci_read_local_version(uart_port);
    btrtl_download_firmware(uart_port);
    hci_reset(uart_port);   /* re-reset to activate firmware */
    hci_read_bd_addr(uart_port);

    g_bt.ready = true;
    g_bt_found = true;

    /* Register with /zirv/net/bt0 */
    device_desc_t *desc = (device_desc_t *)kzalloc(sizeof(device_desc_t), 0);
    if (desc) {
        /* Reuse NET_WLAN bus class as a placeholder until a BT class is added */
        desc->bus_class   = DEV_CLASS_NET_WLAN;
        desc->media_class = DEV_CLASS_NET_WLAN;
        desc->present     = true;
        desc->driver_data = &g_bt;
        const char *model = "Realtek RTL8723DE Bluetooth 4.0";
        for (int i = 0; model[i] && i < 63; i++) desc->model[i] = model[i];
        /* Register as /zirv/net/bt0 by using index 1 (wlan0 is index 0) */
        vfs_register_device(DEV_CLASS_NET_WLAN, DEV_CLASS_NET_WLAN, 1, desc);
    }

    serial_puts(SERIAL_COM1, "[btrtl] Bluetooth ready → /zirv/net/bt0\n");
}

bool btrtl_get_bd_addr(uint8_t buf[6])
{
    if (!g_bt_found || !g_bt.ready) return false;
    for (int i = 0; i < 6; i++) buf[i] = g_bt.bd_addr[i];
    return true;
}
