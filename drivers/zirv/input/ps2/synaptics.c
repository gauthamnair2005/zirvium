/* drivers/zirv/input/ps2/synaptics.c
 * Zirvium Kernel — Synaptics TouchPad driver
 *
 * Ported from the Linux kernel's drivers/input/mouse/synaptics.c logic,
 * re-written to use Zirvium's MOSIX APIs instead of Linux input subsystem.
 */
#include "synaptics.h"
#include "i8042.h"
#include "drivers/zirv/device.h"
#include "drivers/serial/serial.h"
#include "fs/mosix.h"
#include "arch/x64/cpu.h"
#include "drivers/compat/linux_compat.h"
#include "mouse.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Synaptics PS/2 commands ─────────────────────────────────────────────── */
/* "Set resolution" is used to send nibbles */
#define SYN_SET_MODE_BYTE(nibble)  (0xE8), (nibble)

/* Synaptics capability query ID */
#define SYN_ID_MAGIC_BYTE2   0x47

/* Absolute mode byte values */
#define SYN_MODE_ABSOLUTE    0xC0
#define SYN_MODE_WMODE       0x08
#define SYN_MODE_SLEEP       0x08

/* 6-byte absolute packet header bits */
#define SYN_PKT_W_MASK       0x0F
#define SYN_PKT_GESTURE      (1 << 2)

/* ── Ring buffer ─────────────────────────────────────────────────────────── */
static touchpad_event_t tp_ring[TP_RING_SIZE];
static int tp_head = 0, tp_tail = 0;

static void tp_push(touchpad_event_t ev)
{
    int next = (tp_head + 1) % TP_RING_SIZE;
    if (next != tp_tail) {
        tp_ring[tp_head] = ev;
        tp_head = next;
    }
}

/* ── Mouse event ring buffer (relative mode) ─────────────────────────────── */
#define MOUSE_RING_SIZE  16
static mouse_event_t mouse_ring[MOUSE_RING_SIZE];
static int mouse_head = 0, mouse_tail = 0;

static void mouse_push(mouse_event_t ev)
{
    int next = (mouse_head + 1) % MOUSE_RING_SIZE;
    if (next != mouse_tail) {
        mouse_ring[mouse_head] = ev;
        mouse_head = next;
    }
}

static int mouse_read_vfs(vnode_t *vn, void *buf, size_t count, uint64_t off)
{
    (void)vn; (void)off;
    if (count < sizeof(mouse_event_t)) return 0;
    if (mouse_tail == mouse_head) return 0;
    *(mouse_event_t *)buf = mouse_ring[mouse_tail];
    mouse_tail = (mouse_tail + 1) % MOUSE_RING_SIZE;
    return (int)sizeof(mouse_event_t);
}

static const vnode_ops_t mouse_vnode_ops = {
    .read = mouse_read_vfs,
};

/* ── Packet accumulator ─────────────────────────────────────────────────── */
static uint8_t pkt[6];
static int     pkt_idx = 0;
static bool    is_absolute = false;

/* ── Decode a 6-byte Synaptics absolute packet ────────────────────────────
 *
 * Byte layout (Synaptics absolute mode, 6-byte):
 *   [0] W[3:0] | MR[1] | MF[1] | OOW[1] | LR[1]
 *   [1] Y[12:8]| Y[7:0] — split across bytes 1 and 4
 *   [2] X[12:8]| X[7:0] — split across bytes 2 and 5
 *   [3] X[4:1] (high bits of pressure field in W mode)
 *   [4] Y[8:4]
 *   [5] pressure
 */
static void decode_absolute(void)
{
    touchpad_event_t ev;

    ev.x = (int32_t)
           (((uint32_t)(pkt[3] & 0x10) << 8)
          | ((uint32_t)(pkt[1] & 0x0F) << 8)
          |  (uint32_t) pkt[4]);

    ev.y = (int32_t)
           (((uint32_t)(pkt[3] & 0x20) << 7)
          | ((uint32_t)(pkt[1] & 0xF0) << 4)
          |  (uint32_t) pkt[5]);

    /* Byte 0 bit layout for buttons */
    ev.btn_left   = (pkt[0] & 0x01) != 0;
    ev.btn_right  = (pkt[0] & 0x02) != 0;
    ev.btn_middle = (pkt[0] & 0x04) != 0;

    ev.pressure = pkt[2];

    /* Finger count heuristic: W = 0 → palm, W = 4..7 → 1 finger, others → 2 */
    uint8_t w = pkt[0] >> 4;
    ev.fingers = (ev.pressure < 25) ? 0 : (w >= 4 && w <= 7) ? 1 : 2;

    tp_push(ev);
}

/* ── Raw byte handler ─────────────────────────────────────────────────────── */
static int rel_pkt_idx = 0;
static uint8_t rel_pkt[3];

static void aux_byte_handler(uint8_t byte, void *data)
{
    (void)data;

    if (!is_absolute) {
        /* Standard 3-byte PS/2 mouse packet */
        if (rel_pkt_idx == 0 && !(byte & 0x08)) return;
        rel_pkt[rel_pkt_idx++] = byte;
        if (rel_pkt_idx == 3) {
            int32_t dx = rel_pkt[1];
            int32_t dy = rel_pkt[2];
            if (rel_pkt[0] & 0x10) dx -= 256;
            if (rel_pkt[0] & 0x20) dy -= 256;
            mouse_event_t ev = { .dx = dx, .dy = -dy, .buttons = rel_pkt[0] & 0x07 };
            mouse_push(ev);
            rel_pkt_idx = 0;
        }
        return;
    }

    /* Sync detection: bit 2 of byte 0 must be 1, bits 7:6 must be 10 */
    if (pkt_idx == 0 && (byte & 0xC8) != 0x80)
        return;

    pkt[pkt_idx++] = byte;
    if (pkt_idx == 6) {
        decode_absolute();
        pkt_idx = 0;
    }
}

/* ── Send a "set resolution nibble" sequence ──────────────────────────────── */
static void syn_send_nibbles(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
    i8042_send_aux(0xE8); i8042_send_aux(b0);
    i8042_send_aux(0xE8); i8042_send_aux(b1);
    i8042_send_aux(0xE8); i8042_send_aux(b2);
    i8042_send_aux(0xE8); i8042_send_aux(b3);
}

/* ── Public API ───────────────────────────────────────────────────────────── */
bool synaptics_init(void)
{
    serial_puts(SERIAL_COM1, "[synaptics] Probing for Synaptics touchpad\n");

    /* Disable stream mode */
    i8042_send_aux(0xF5);
    msleep(10);

    /* Identify Synaptics: send magic nibble sequence then E9 (status request) */
    syn_send_nibbles(0x00, 0x00, 0x00, 0x00);
    i8042_send_aux(0xE9);
    msleep(10);

    /* The i8042 aux handler will receive 3 bytes here.
     * For detection purposes we check the identity byte directly by
     * reading the aux port via the status register. */

    /* Flush and try to read 3 identify bytes */
    uint8_t id[3] = {0, 0, 0};
    for (int b = 0; b < 3; b++) {
        for (int t = 0; t < 10000; t++) {
            if (inb(0x64) & 0x21) { /* OBF set */
                id[b] = inb(0x60);
                break;
            }
            udelay(100);
        }
    }

    if (id[1] != SYN_ID_MAGIC_BYTE2) {
        serial_puts(SERIAL_COM1, "[synaptics] Not found - registering as generic PS/2 mouse\n");
        i8042_send_aux(0xF4);   /* enable */
        i8042_register_port_handler(I8042_PORT_AUX, aux_byte_handler, NULL);

        device_desc_t *mdesc = (device_desc_t *)kzalloc(sizeof(device_desc_t), 0);
        if (mdesc) {
            mdesc->bus_class   = DEV_CLASS_INPUT_MOUSE;
            mdesc->media_class = DEV_CLASS_INPUT_MOUSE;
            mdesc->present     = true;
            const char *model = "PS/2 Mouse";
            for (int i = 0; model[i] && i < 63; i++) mdesc->model[i] = model[i];
            vnode_t *mouse_vnode = vfs_register_device(DEV_CLASS_INPUT_MOUSE,
                                        DEV_CLASS_INPUT_MOUSE, 0, mdesc);
            if (mouse_vnode)
                mouse_vnode->ops = &mouse_vnode_ops;
        }
        return false;
    }

    serial_puts(SERIAL_COM1, "[synaptics] Synaptics device detected\n");

    /* Enter absolute W mode:
     *  mode byte = 0xC8 (absolute | W mode)
     *  nibble sequence: E8 C8 E8 00 E8 28 E8 C8
     *  then set stream mode (F4)
     */
    syn_send_nibbles(0x0C, 0x00, 0x02, 0x0C);   /* = 0xC0 | 0x00 | 0x28 | 0xC0 → absolute */
    i8042_send_aux(0xF3);   /* set sample rate */
    i8042_send_aux(0x14);   /* 20 samples/sec  */
    i8042_send_aux(0xF4);   /* enable           */

    is_absolute = true;
    pkt_idx     = 0;

    i8042_register_port_handler(I8042_PORT_AUX, aux_byte_handler, NULL);

    /* Register with /zirv/input/touchpad0 */
    device_desc_t *desc = (device_desc_t *)kzalloc(sizeof(device_desc_t), 0);
    if (desc) {
        desc->bus_class   = DEV_CLASS_INPUT_TOUCHPAD;
        desc->media_class = DEV_CLASS_INPUT_TOUCHPAD;
        desc->present     = true;
        const char *model = "Synaptics PS/2 TouchPad";
        for (int i = 0; model[i] && i < 63; i++) desc->model[i] = model[i];
        vfs_register_device(DEV_CLASS_INPUT_TOUCHPAD,
                            DEV_CLASS_INPUT_TOUCHPAD, 0, desc);
    }

    serial_puts(SERIAL_COM1, "[synaptics] Touchpad in absolute mode → /zirv/input/touchpad0\n");
    return true;
}

bool synaptics_read_event(touchpad_event_t *out)
{
    if (tp_tail == tp_head) return false;
    *out = tp_ring[tp_tail];
    tp_tail = (tp_tail + 1) % TP_RING_SIZE;
    return true;
}

int synaptics_pending(void)
{
    return (tp_head - tp_tail + TP_RING_SIZE) % TP_RING_SIZE;
}

int mouse_read_event(mouse_event_t *ev)
{
    if (!ev) return -1;
    if (mouse_tail == mouse_head) return -1;
    *ev = mouse_ring[mouse_tail];
    mouse_tail = (mouse_tail + 1) % MOUSE_RING_SIZE;
    return 0;
}
