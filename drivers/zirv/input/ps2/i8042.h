/* drivers/zirv/input/ps2/i8042.h
 * Zirvium Kernel — i8042 PS/2 Controller driver
 *
 * The Intel 8042 (i8042) is the PS/2 controller found in virtually every
 * x86 laptop.  It manages two PS/2 ports:
 *   Port 1 (IRQ 1)  — keyboard
 *   Port 2 (IRQ 12) — auxiliary device (touchpad / mouse)
 *
 * This driver initialises the controller and registers handlers for both
 * ports.  Higher-level drivers (keyboard decoder, Synaptics touchpad) call
 * i8042_register_port_handler() to receive scan-codes / packets.
 */
#ifndef ZIRVIUM_DRIVERS_INPUT_PS2_I8042_H
#define ZIRVIUM_DRIVERS_INPUT_PS2_I8042_H

#include <stdint.h>
#include <stdbool.h>

/* ── i8042 I/O ports ─────────────────────────────────────────────────────── */
#define I8042_DATA_PORT    0x60   /* read/write data */
#define I8042_STATUS_PORT  0x64   /* read  = status register */
#define I8042_CMD_PORT     0x64   /* write = command register */

/* ── Status register bits ────────────────────────────────────────────────── */
#define I8042_STS_OBF   (1 << 0)   /* Output Buffer Full (data to read) */
#define I8042_STS_IBF   (1 << 1)   /* Input  Buffer Full (busy, don't write) */
#define I8042_STS_SYS   (1 << 2)   /* System flag */
#define I8042_STS_A2    (1 << 3)   /* Command/Data (0=data, 1=command) */
#define I8042_STS_INH   (1 << 4)   /* Inhibit flag */
#define I8042_STS_MOBF  (1 << 5)   /* Mouse Output Buffer Full */
#define I8042_STS_TO    (1 << 6)   /* Timeout error */
#define I8042_STS_PERR  (1 << 7)   /* Parity error */

/* ── Controller commands (sent to I8042_CMD_PORT) ────────────────────────── */
#define I8042_CMD_READ_CONFIG   0x20   /* Read configuration byte */
#define I8042_CMD_WRITE_CONFIG  0x60   /* Write configuration byte */
#define I8042_CMD_DISABLE_PORT2 0xA7
#define I8042_CMD_ENABLE_PORT2  0xA8
#define I8042_CMD_TEST_PORT2    0xA9
#define I8042_CMD_SELF_TEST     0xAA
#define I8042_CMD_TEST_PORT1    0xAB
#define I8042_CMD_DISABLE_PORT1 0xAD
#define I8042_CMD_ENABLE_PORT1  0xAE
#define I8042_CMD_READ_INPUT    0xC0
#define I8042_CMD_WRITE_AUX     0xD4   /* Next byte goes to aux (port 2) */

/* ── Configuration byte bits ─────────────────────────────────────────────── */
#define I8042_CFG_PORT1_INT  (1 << 0)   /* Port 1 interrupt enable */
#define I8042_CFG_PORT2_INT  (1 << 1)   /* Port 2 interrupt enable */
#define I8042_CFG_SYS_FLAG   (1 << 2)
#define I8042_CFG_PORT1_CLK  (1 << 4)   /* Port 1 clock disable */
#define I8042_CFG_PORT2_CLK  (1 << 5)   /* Port 2 clock disable */
#define I8042_CFG_XLAT       (1 << 6)   /* Scancode translation */

/* ── Port IDs ────────────────────────────────────────────────────────────── */
#define I8042_PORT_KBD  1
#define I8042_PORT_AUX  2

/* ── Callback type ───────────────────────────────────────────────────────── */
typedef void (*i8042_handler_t)(uint8_t byte, void *data);

/* ── Public API ───────────────────────────────────────────────────────────── */

/** i8042_init - initialise and self-test the i8042 controller.
 *  Returns true if successful. */
bool i8042_init(void);

/** i8042_register_port_handler - register a callback for raw PS/2 bytes.
 *  @port:    I8042_PORT_KBD or I8042_PORT_AUX
 *  @handler: called for every byte received on that port
 *  @data:    opaque pointer passed to handler */
void i8042_register_port_handler(int port, i8042_handler_t handler, void *data);

/** i8042_send_kbd - send one byte to the keyboard.
 *  Returns true if the device ACKed (0xFA). */
bool i8042_send_kbd(uint8_t byte);

/** i8042_send_aux - send one byte to the auxiliary (touchpad/mouse) port.
 *  Returns true if the device ACKed. */
bool i8042_send_aux(uint8_t byte);

/** i8042_flush - flush the output buffer (discard pending bytes). */
void i8042_flush(void);

#endif /* ZIRVIUM_DRIVERS_INPUT_PS2_I8042_H */
