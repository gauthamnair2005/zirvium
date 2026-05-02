/* drivers/zirv/wifi/rtl8723de/rtl8723de.h
 * Zirvium Kernel — Realtek RTL8723DE PCIe WiFi+BT 4.0 driver
 *
 * The RTL8723DE is an 802.11bgn single-band (2.4 GHz) + Bluetooth 4.0
 * combo chip connected via PCIe.
 *
 * PCI IDs:
 *   Vendor  0x10EC (Realtek)
 *   Device  0xD723 (RTL8723DE)
 *
 * The chip exposes:
 *  BAR 0 — 16 KiB MMIO register space (WiFi MAC + baseband)
 *  BAR 2 — 256 B   I/O register space (legacy)
 *
 * Firmware:   rtlwifi/rtl8723defw.bin  (loaded from /boot/firmware/)
 * Linux ref:  drivers/net/wireless/realtek/rtlwifi/rtl8723de/
 *
 * This driver is ported to Zirvium's MOSIX APIs.  The compat layer in
 * drivers/compat/linux_compat.h maps Linux types and macros to Zirvium.
 */
#ifndef ZIRVIUM_DRIVERS_WIFI_RTL8723DE_H
#define ZIRVIUM_DRIVERS_WIFI_RTL8723DE_H

#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"

/* ── PCI identification ──────────────────────────────────────────────────── */
#define RTL8723DE_VENDOR_ID  0x10EC
#define RTL8723DE_DEVICE_ID  0xD723

/* ── MMIO register offsets (BAR0) ────────────────────────────────────────── */
/* System function */
#define RTL_SYS_FUNC_EN      0x02   /* System Function Enable */
#define RTL_SYS_CLKR         0x08   /* System Clock Register  */
#define RTL_APS_FSMCO        0x04   /* Auto Power Save FSM Control */
#define RTL_SYS_ISO_CTRL     0x00   /* System Isolation Control */
#define RTL_CR               0x100  /* Command Register */
#define RTL_TCR              0x604  /* Transmit Configuration */
#define RTL_RCR              0x608  /* Receive  Configuration */
#define RTL_RRSR             0x60C  /* Response Rate Set */
#define RTL_RATR0            0x320  /* Rate Adaptive Table */

/* Command Register bits */
#define RTL_CR_RST           BIT(4)
#define RTL_CR_RE            BIT(3)
#define RTL_CR_TE            BIT(2)

/* APS_FSMCO bits */
#define RTL_APFM_ONMAC       BIT(8)

/* SYS_FUNC_EN bits */
#define RTL_FEN_MREGEN       BIT(15)
#define RTL_FEN_DCORE        BIT(3)
#define RTL_FEN_USBDA        BIT(2)

/* Interrupt registers */
#define RTL_HIMR             0x0B0  /* Host Interrupt Mask    */
#define RTL_HISR             0x0B4  /* Host Interrupt Status  */

/* HISR / HIMR bits */
#define RTL_HISR_TXERR       BIT(11)
#define RTL_HISR_RXERR       BIT(10)
#define RTL_HISR_ROK         BIT(3)   /* RX OK */
#define RTL_HISR_TOK         BIT(0)   /* TX OK */

/* MAC address registers */
#define RTL_MAC0             0x610
#define RTL_MAC4             0x614

/* ── Firmware ────────────────────────────────────────────────────────────── */
#define RTL8723DE_FW_PATH    "/boot/firmware/rtl8723defw.bin"
#define RTL8723DE_FW_MAX_LEN (32 * 1024)

/* ── Driver state ────────────────────────────────────────────────────────── */
typedef struct {
    pci_dev_t *pdev;
    void      *mmio;          /* BAR0 virtual address */
    uint8_t    mac_addr[6];
    bool       fw_loaded;
    bool       hw_ready;
} rtl8723de_priv_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/** rtl8723de_init - probe PCIe bus and initialise RTL8723DE if found.
 *  Registers the device under /zirv/net/wlan0. */
void rtl8723de_init(void);

/** rtl8723de_get_mac - copy the 6-byte MAC address into @buf. */
bool rtl8723de_get_mac(uint8_t buf[6]);

#endif /* ZIRVIUM_DRIVERS_WIFI_RTL8723DE_H */
