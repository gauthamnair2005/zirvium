/* drivers/zirv/input/ps2/i8042.c
 * Zirvium Kernel — i8042 PS/2 Controller implementation
 */
#include "i8042.h"
#include "kernel/irq/irq.h"
#include "arch/x64/cpu.h"
#include "drivers/zirv/device.h"
#include "drivers/serial/serial.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Low-level i8042 helpers ─────────────────────────────────────────────── */

static bool i8042_wait_write(void)
{
    for (int i = 0; i < 100000; i++) {
        if (!(inb(I8042_STATUS_PORT) & I8042_STS_IBF))
            return true;
        cpu_pause();
    }
    return false;
}

static bool i8042_wait_read(void)
{
    for (int i = 0; i < 100000; i++) {
        if (inb(I8042_STATUS_PORT) & I8042_STS_OBF)
            return true;
        cpu_pause();
    }
    return false;
}

static void i8042_send_cmd(uint8_t cmd)
{
    i8042_wait_write();
    outb(I8042_CMD_PORT, cmd);
}

static uint8_t i8042_read(void)
{
    i8042_wait_read();
    return inb(I8042_DATA_PORT);
}

static void i8042_write_data(uint8_t data)
{
    i8042_wait_write();
    outb(I8042_DATA_PORT, data);
}

/* ── Per-port handler storage ─────────────────────────────────────────────── */
static i8042_handler_t kbd_handler;
static void           *kbd_handler_data;
static i8042_handler_t aux_handler;
static void           *aux_handler_data;

/* ── IRQ handlers ─────────────────────────────────────────────────────────── */
static int kbd_irq_handler(int irq, void *data)
{
    (void)irq; (void)data;
    uint8_t status = inb(I8042_STATUS_PORT);
    if (!(status & I8042_STS_OBF)) return IRQ_NONE;
    /* Mouse data comes on IRQ1 occasionally if MOBF is set too */
    if (status & I8042_STS_MOBF) {
        uint8_t byte = inb(I8042_DATA_PORT);
        if (aux_handler) aux_handler(byte, aux_handler_data);
    } else {
        uint8_t byte = inb(I8042_DATA_PORT);
        if (kbd_handler) kbd_handler(byte, kbd_handler_data);
    }
    return IRQ_HANDLED;
}

static int aux_irq_handler(int irq, void *data)
{
    (void)irq; (void)data;
    uint8_t status = inb(I8042_STATUS_PORT);
    if (!(status & I8042_STS_OBF)) return IRQ_NONE;
    uint8_t byte = inb(I8042_DATA_PORT);
    if (aux_handler) aux_handler(byte, aux_handler_data);
    return IRQ_HANDLED;
}

/* ── Public API ───────────────────────────────────────────────────────────── */
void i8042_flush(void)
{
    for (int i = 0; i < 16; i++) {
        if (!(inb(I8042_STATUS_PORT) & I8042_STS_OBF)) break;
        inb(I8042_DATA_PORT);
    }
}

bool i8042_init(void)
{
    serial_puts(SERIAL_COM1, "[i8042] Initialising PS/2 controller\n");

    /* Disable both ports while setting up */
    i8042_send_cmd(I8042_CMD_DISABLE_PORT1);
    i8042_send_cmd(I8042_CMD_DISABLE_PORT2);
    i8042_flush();

    /* Read and modify configuration byte */
    i8042_send_cmd(I8042_CMD_READ_CONFIG);
    uint8_t cfg = i8042_read();

    /* Disable IRQs and translation during init */
    cfg &= (uint8_t)~(I8042_CFG_PORT1_INT | I8042_CFG_PORT2_INT | I8042_CFG_XLAT);
    i8042_send_cmd(I8042_CMD_WRITE_CONFIG);
    i8042_write_data(cfg);

    /* Self-test */
    i8042_send_cmd(I8042_CMD_SELF_TEST);
    uint8_t result = i8042_read();
    if (result != 0x55) {
        serial_puts(SERIAL_COM1, "[i8042] Self-test FAILED\n");
        return false;
    }

    /* Test port 1 (keyboard) */
    i8042_send_cmd(I8042_CMD_TEST_PORT1);
    result = i8042_read();
    bool port1_ok = (result == 0x00);

    /* Test port 2 (aux / touchpad) */
    i8042_send_cmd(I8042_CMD_TEST_PORT2);
    result = i8042_read();
    bool port2_ok = (result == 0x00);

    /* Enable working ports + interrupts */
    if (port1_ok) {
        i8042_send_cmd(I8042_CMD_ENABLE_PORT1);
        cfg |= I8042_CFG_PORT1_INT;
    }
    if (port2_ok) {
        i8042_send_cmd(I8042_CMD_ENABLE_PORT2);
        cfg |= I8042_CFG_PORT2_INT;
    }
    i8042_send_cmd(I8042_CMD_WRITE_CONFIG);
    i8042_write_data(cfg);

    /* Register IRQ handlers */
    if (port1_ok)
        request_irq(IRQ_KEYBOARD,   kbd_irq_handler, IRQF_SHARED,
                    "i8042-kbd", NULL);
    if (port2_ok)
        request_irq(IRQ_PS2_MOUSE,  aux_irq_handler, IRQF_SHARED,
                    "i8042-aux", NULL);

    serial_puts(SERIAL_COM1, "[i8042] PS/2 controller ready\n");
    return true;
}

void i8042_register_port_handler(int port, i8042_handler_t handler, void *data)
{
    if (port == I8042_PORT_KBD) {
        kbd_handler      = handler;
        kbd_handler_data = data;
    } else if (port == I8042_PORT_AUX) {
        aux_handler      = handler;
        aux_handler_data = data;
    }
}

bool i8042_send_kbd(uint8_t byte)
{
    for (int retry = 0; retry < 3; retry++) {
        i8042_write_data(byte);
        uint8_t ack = i8042_read();
        if (ack == 0xFA) return true;
        if (ack == 0xFE) continue;   /* resend */
        break;
    }
    return false;
}

bool i8042_send_aux(uint8_t byte)
{
    for (int retry = 0; retry < 3; retry++) {
        i8042_send_cmd(I8042_CMD_WRITE_AUX);
        i8042_write_data(byte);
        uint8_t ack = i8042_read();
        if (ack == 0xFA) return true;
        if (ack == 0xFE) continue;
        break;
    }
    return false;
}
