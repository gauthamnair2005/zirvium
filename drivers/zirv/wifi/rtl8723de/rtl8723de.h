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

/* ── TX/RX DMA ring descriptors ─────────────────────────────────────────── */
/*
 * Descriptor layout derived from Linux rtlwifi/rtl8723de/ and the Realtek
 * RTL8723D datasheet.  Each descriptor is 64 bytes (16 DWORDs).
 *
 * OWN bit (DW0[31]): 1 = owned by the NIC DMA engine, 0 = owned by the driver.
 * EOR bit (DW0[26]): 1 = last descriptor in the ring (wrap around).
 */
#define RTL_TX_DESC_DW       16          /* DWORDs per TX descriptor */
#define RTL_RX_DESC_DW        8          /* DWORDs per RX descriptor */
#define RTL_TX_RING_SIZE     32          /* TX ring depth (must be power of 2) */
#define RTL_RX_RING_SIZE     32          /* RX ring depth (must be power of 2) */
#define RTL_RX_BUF_SIZE    1536          /* bytes per RX buffer (≥ max MPDU) */

typedef struct __attribute__((packed)) {
    uint32_t dw0;   /* [31]=OWN [30]=LINIP [29]=NO_ACM [28]=GF [26]=EOR
                     * [25]=FS [24]=LS [23]=HTCF [22]=AMSDU [21]=AMLDU
                     * [20]=MOREDATA [17:16]=pkt_offset [15:0]=txpktsize */
    uint32_t dw1;   /* [31:26]=macid [25:24]=pktoff [21:16]=qsel
                     * [15]=RDG_NAUDMAEN [12]=NAVUSEHDR [11]=EN_DESC_ID
                     * [10]=SEC_TYPE[1] [9]=SEC_TYPE[0] [7:0]=raid */
    uint32_t dw2;   /* [31:27]=ampdu_density [26]=bmc [25]=htc
                     * [23:16]=ampdu_factor [7:0]=max_agg_num */
    uint32_t dw3;   /* [31:16]=nextheadpage [15:8]=tailpage [7:0]=tx_rate */
    uint32_t dw4;   /* rate/retry info */
    uint32_t dw5;   /* more rate/retry info */
    uint32_t dw6;   /* reserved */
    uint32_t dw7;   /* reserved */
    uint32_t dw8;   /* [15:0]=txbufsize */
    uint32_t dw9;   /* reserved */
    uint32_t dw10;  /* reserved */
    uint32_t dw11;  /* reserved */
    uint32_t dw12;  /* next descriptor physical address (low 32 bits) */
    uint32_t dw13;  /* next descriptor physical address (high 32 bits) */
    uint32_t dw14;  /* TX buffer physical address (low 32 bits) */
    uint32_t dw15;  /* TX buffer physical address (high 32 bits) */
} rtl_tx_desc_t;

typedef struct __attribute__((packed)) {
    uint32_t dw0;   /* [31]=OWN [28]=EOR [26]=ICVERR [25]=RXERR
                     * [24]=CRCERR [13:0]=pkt_len */
    uint32_t dw1;   /* [25:20]=macid [17:16]=rxrate_idx [6]=hwrsvd
                     * [5]=chkerr [4]=ipv6 [3]=tcpchkrpt [2]=udpchkrpt
                     * [1]=ipchkrpt [0]=paggr */
    uint32_t dw2;   /* [31:16]=seq_num [15:0]=frag_num */
    uint32_t dw3;   /* timestamp low */
    uint32_t dw4;   /* timestamp high / RSSI */
    uint32_t dw5;   /* TSFL (timestamp for localtime) */
    uint32_t dw6;   /* RX buffer physical address (low 32 bits) */
    uint32_t dw7;   /* RX buffer physical address (high 32 bits) */
} rtl_rx_desc_t;

/* Bit helpers for DW0 */
#define RTL_DESC_OWN         (1u << 31)
#define RTL_DESC_EOR         (1u << 26)
#define RTL_TX_PKTSIZE_MASK  0x0000FFFFu
#define RTL_RX_PKTSIZE_MASK  0x00003FFFu

/* TX queue DMA address registers (RTL8723DE) */
#define RTL_TDLSA            0x520   /* TX low-priority queue descriptor start */
#define RTL_BDQDA            0x528   /* Beacon queue descriptor start */
#define RTL_RDQDA            0x518   /* TX management queue descriptor start */

/* RX queue DMA address register */
#define RTL_RDQSA            0x5F4   /* RX queue descriptor start address */

/* ── Driver state ────────────────────────────────────────────────────────── */
typedef struct {
    pci_dev_t *pdev;
    void      *mmio;          /* BAR0 virtual address */
    uint8_t    mac_addr[6];
    bool       fw_loaded;
    bool       hw_ready;

    /* TX ring */
    rtl_tx_desc_t *tx_ring;        /* virtual address */
    uint64_t       tx_ring_phys;   /* physical address */
    uint32_t       tx_head;        /* next desc to give to NIC */
    uint32_t       tx_tail;        /* next desc filled by driver */

    /* RX ring */
    rtl_rx_desc_t *rx_ring;        /* virtual address */
    uint64_t       rx_ring_phys;
    void          *rx_bufs[RTL_RX_RING_SIZE];  /* virtual pointers to RX data */
    uint64_t       rx_bufs_phys[RTL_RX_RING_SIZE];
    uint32_t       rx_head;        /* next desc to hand back to NIC */
} rtl8723de_priv_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/** rtl8723de_init - probe PCIe bus and initialise RTL8723DE if found.
 *  Registers the device under /zirv/net/wlan0. */
void rtl8723de_init(void);

/** rtl8723de_get_mac - copy the 6-byte MAC address into @buf. */
bool rtl8723de_get_mac(uint8_t buf[6]);

/**
 * rtl8723de_send - queue a raw 802.11 frame for transmission.
 * @data: pointer to the frame bytes (driver copies internally)
 * @len:  frame length in bytes (must be ≤ 1500)
 * Returns true on success, false if the TX ring is full or the device is
 * not ready.
 */
bool rtl8723de_send(const void *data, uint16_t len);

#endif /* ZIRVIUM_DRIVERS_WIFI_RTL8723DE_H */
