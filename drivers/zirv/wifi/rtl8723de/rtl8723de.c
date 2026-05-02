/* drivers/zirv/wifi/rtl8723de/rtl8723de.c
 * Zirvium Kernel — RTL8723DE PCIe WiFi driver
 *
 * Ported from Linux drivers/net/wireless/realtek/rtlwifi/rtl8723de/
 * (GPL-2.0, Copyright Realtek Semiconductor Corp.)
 *
 * Re-written to use Zirvium MOSIX APIs.  Hardware initialisation sequence
 * and register definitions are derived from the publicly available Realtek
 * Linux driver source and RTL8723D hardware design guide.
 */
#include "rtl8723de.h"
#include "drivers/pci/pci.h"
#include "drivers/zirv/device.h"
#include "drivers/serial/serial.h"
#include "kernel/irq/irq.h"
#include "drivers/compat/linux_compat.h"

/* ── Static device state ──────────────────────────────────────────────────── */
static rtl8723de_priv_t g_priv;
static bool g_found = false;

/* ── MMIO helpers ─────────────────────────────────────────────────────────── */
static inline u32 rtl_read32(rtl8723de_priv_t *p, u32 reg)
{
    return readl((uint8_t *)p->mmio + reg);
}
static inline void rtl_write32(rtl8723de_priv_t *p, u32 reg, u32 val)
{
    writel(val, (uint8_t *)p->mmio + reg);
}
static inline u16 rtl_read16(rtl8723de_priv_t *p, u32 reg)
{
    return readw((uint8_t *)p->mmio + reg);
}
static inline void rtl_write16(rtl8723de_priv_t *p, u32 reg, u16 val)
{
    writew(val, (uint8_t *)p->mmio + reg);
}
static inline u8 rtl_read8(rtl8723de_priv_t *p, u32 reg)
{
    return readb((uint8_t *)p->mmio + reg);
}
static inline void rtl_write8(rtl8723de_priv_t *p, u32 reg, u8 val)
{
    writeb(val, (uint8_t *)p->mmio + reg);
}

/* ── Firmware loading ─────────────────────────────────────────────────────── */
/* The firmware binary lives at /boot/firmware/rtl8723defw.bin.
 * We load it via the VFS once the filesystem is mounted.  During the
 * initial boot we skip firmware loading and mark the device as partially
 * initialised — a later boot stage will call rtl8723de_load_firmware().
 *
 * Firmware download protocol (simplified from Linux driver):
 *  1. Write firmware to the FW download page via RTL_FWDL registers.
 *  2. Set FW_DL_EN bit and wait for the CPU to report ready.
 */
#define RTL_FWDL_CTRL         0x1080   /* Firmware Download Control */
#define RTL_FWDL_EN           BIT(0)
#define RTL_FWDL_CHKSUM_RPT   BIT(1)
#define RTL_CPU_OPT_EN        BIT(2)
#define RTL_WINTINI_RDY       BIT(6)

static bool rtl8723de_fw_check_ready(rtl8723de_priv_t *p)
{
    for (int i = 0; i < 1000; i++) {
        u8 val = rtl_read8(p, RTL_FWDL_CTRL);
        if (val & RTL_WINTINI_RDY) return true;
        udelay(100);
    }
    return false;
}

/* ── MAC address read ────────────────────────────────────────────────────── */
static void rtl8723de_read_mac(rtl8723de_priv_t *p)
{
    u32 mac0 = rtl_read32(p, RTL_MAC0);
    u16 mac4 = rtl_read16(p, RTL_MAC4);
    p->mac_addr[0] = (u8)( mac0        & 0xFF);
    p->mac_addr[1] = (u8)((mac0 >>  8) & 0xFF);
    p->mac_addr[2] = (u8)((mac0 >> 16) & 0xFF);
    p->mac_addr[3] = (u8)((mac0 >> 24) & 0xFF);
    p->mac_addr[4] = (u8)( mac4        & 0xFF);
    p->mac_addr[5] = (u8)((mac4 >>  8) & 0xFF);
}

/* ── Hardware initialisation ─────────────────────────────────────────────── */
static int rtl8723de_hw_init(rtl8723de_priv_t *p)
{
    /* Step 1: Power on sequence — enable digital core and MAC */
    u16 func_en = rtl_read16(p, RTL_SYS_FUNC_EN);
    func_en |= RTL_FEN_MREGEN;
    rtl_write16(p, RTL_SYS_FUNC_EN, func_en);
    udelay(20);

    func_en |= RTL_FEN_DCORE;
    rtl_write16(p, RTL_SYS_FUNC_EN, func_en);
    udelay(20);

    /* Step 2: Enable MAC clock */
    u32 aps = rtl_read32(p, RTL_APS_FSMCO);
    aps |= RTL_APFM_ONMAC;
    rtl_write32(p, RTL_APS_FSMCO, aps);

    /* Wait for MAC to indicate ready */
    for (int i = 0; i < 5000; i++) {
        if (!(rtl_read32(p, RTL_APS_FSMCO) & RTL_APFM_ONMAC))
            break;
        udelay(10);
    }

    /* Step 3: Load firmware (best-effort; skip if not available at boot) */
    if (rtl8723de_fw_check_ready(p)) {
        p->fw_loaded = true;
        serial_puts(SERIAL_COM1, "[rtl8723de] Firmware already loaded by BIOS\n");
    } else {
        serial_puts(SERIAL_COM1,
            "[rtl8723de] Firmware not loaded — will retry after VFS mount\n");
        /* The firmware loader will call rtl8723de_hw_init() again once
         * /boot is mounted and rtl8723defw.bin is accessible. */
    }

    /* Step 4: Reset and enable RX/TX */
    rtl_write8(p, RTL_CR, RTL_CR_RST);
    udelay(100);

    /* Step 5: Read MAC address burned in efuse */
    rtl8723de_read_mac(p);

    /* Step 6: Enable interrupts (ROK + TOK) */
    rtl_write32(p, RTL_HISR, 0xFFFFFFFF);   /* clear pending */
    rtl_write32(p, RTL_HIMR, RTL_HISR_ROK | RTL_HISR_TOK
                             | RTL_HISR_RXERR | RTL_HISR_TXERR);

    p->hw_ready = true;
    return 0;
}

/* ── IRQ handler ──────────────────────────────────────────────────────────── */
static int rtl8723de_irq_handler(int irq, void *data)
{
    rtl8723de_priv_t *p = (rtl8723de_priv_t *)data;
    if (!p || !p->mmio) return IRQ_NONE;

    u32 status = rtl_read32(p, RTL_HISR);
    if (!status) return IRQ_NONE;

    /* Acknowledge */
    rtl_write32(p, RTL_HISR, status);

    if (status & RTL_HISR_ROK) {
        /* TODO: process received frames from the RX ring */
    }
    if (status & RTL_HISR_TOK) {
        /* TODO: advance the TX ring, wake any waiting sends */
    }
    if (status & (RTL_HISR_RXERR | RTL_HISR_TXERR)) {
        serial_puts(SERIAL_COM1, "[rtl8723de] DMA error — reset pending\n");
    }

    return IRQ_HANDLED;
}

/* ── Public API ───────────────────────────────────────────────────────────── */
void rtl8723de_init(void)
{
    serial_puts(SERIAL_COM1, "[rtl8723de] Scanning PCI for RTL8723DE\n");

    pci_dev_t *pdev = pci_find_device(RTL8723DE_VENDOR_ID, RTL8723DE_DEVICE_ID);
    if (!pdev) {
        serial_puts(SERIAL_COM1, "[rtl8723de] Not found\n");
        return;
    }

    serial_puts(SERIAL_COM1, "[rtl8723de] Found RTL8723DE\n");
    pci_enable_device(pdev);

    void *mmio = pci_map_bar(pdev, 0);
    if (!mmio) {
        serial_puts(SERIAL_COM1, "[rtl8723de] Failed to map BAR0\n");
        return;
    }

    g_priv.pdev = pdev;
    g_priv.mmio = mmio;

    if (rtl8723de_hw_init(&g_priv) != 0) {
        serial_puts(SERIAL_COM1, "[rtl8723de] HW init failed\n");
        return;
    }

    /* Install IRQ handler */
    request_irq(pdev->irq_line, rtl8723de_irq_handler,
                IRQF_SHARED, "rtl8723de", &g_priv);

    /* Register with /zirv/net/wlan0 */
    device_desc_t *desc = (device_desc_t *)kzalloc(sizeof(device_desc_t), 0);
    if (desc) {
        desc->bus_class   = DEV_CLASS_SATA;    /* no PCIe bus class yet */
        desc->media_class = DEV_CLASS_NET_WLAN;
        desc->present     = true;
        desc->driver_data = &g_priv;
        const char *model = "Realtek RTL8723DE 802.11bgn WiFi";
        for (int i = 0; model[i] && i < 63; i++) desc->model[i] = model[i];
        vfs_register_device(DEV_CLASS_NET_WLAN, DEV_CLASS_NET_WLAN, 0, desc);
    }

    serial_puts(SERIAL_COM1,
        "[rtl8723de] WiFi ready → /zirv/net/wlan0\n");
    g_found = true;
}

bool rtl8723de_get_mac(uint8_t buf[6])
{
    if (!g_found || !g_priv.hw_ready) return false;
    for (int i = 0; i < 6; i++) buf[i] = g_priv.mac_addr[i];
    return true;
}
