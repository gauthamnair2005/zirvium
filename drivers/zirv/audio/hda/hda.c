/* drivers/zirv/audio/hda/hda.c
 * Zirvium Kernel — Intel High Definition Audio (HDA / cAVS) driver
 *
 * Ported from Linux sound/pci/hda/hda_intel.c and sound/hda/hdac_controller.c
 * (GPL-2.0, Copyright Intel Corp. and contributors).
 *
 * Re-implemented using Zirvium MOSIX APIs and the Immediate Command Interface
 * (ICI) for codec communication.
 *
 * This file handles:
 *  1. PCI probe and MMIO BAR mapping
 *  2. Controller reset and codec enumeration
 *  3. Audio Function Group (AFG) discovery via HDA GET_PARAM verbs
 *  4. Output pin and converter configuration
 *  5. BDL / stream descriptor setup for PCM playback
 *  6. Device registration under /zirv/audio/output0
 */
#include "hda.h"
#include "drivers/pci/pci.h"
#include "drivers/zirv/device.h"
#include "drivers/serial/serial.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "drivers/compat/linux_compat.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Static device state ─────────────────────────────────────────────────── */
static hda_priv_t g_hda;
static bool       g_hda_found = false;

/* ── Supported Intel PCH HDA device IDs ─────────────────────────────────── */
static const uint16_t hda_device_ids[] = {
    0x9D71,   /* Kaby Lake-LP  PCH */
    0xA171,   /* Kaby Lake-H   PCH */
    0x9DC8,   /* Coffee Lake-LP PCH */
    0xA348,   /* Coffee Lake-H  PCH */
    0x34C8,   /* Ice Lake-LP   PCH */
    0x43C8,   /* Tiger Lake    PCH */
    0x7AD0,   /* Alder Lake    PCH */
    0x51C8,   /* Alder Lake-P  PCH */
    0x54C8,   /* Alder Lake-N  PCH */
    0xA828,   /* Raptor Lake   PCH */
    0x7E28,   /* Meteor Lake   PCH */
};

/* ── MMIO helpers ─────────────────────────────────────────────────────────── */
static inline uint32_t hda_read32(uint32_t reg)
{
    return readl((uint8_t *)g_hda.mmio + reg);
}
static inline void hda_write32(uint32_t reg, uint32_t val)
{
    writel(val, (uint8_t *)g_hda.mmio + reg);
}
static inline uint16_t hda_read16(uint32_t reg)
{
    return readw((uint8_t *)g_hda.mmio + reg);
}
static inline void hda_write16(uint32_t reg, uint16_t val)
{
    writew(val, (uint8_t *)g_hda.mmio + reg);
}
static inline uint8_t hda_read8(uint32_t reg)
{
    return readb((uint8_t *)g_hda.mmio + reg);
}
static inline void hda_write8(uint32_t reg, uint8_t val)
{
    writeb(val, (uint8_t *)g_hda.mmio + reg);
}

/* ── Immediate Command Interface ─────────────────────────────────────────── */
/*
 * Send a single 32-bit HDA verb and return the 32-bit response.
 * Returns 0xFFFFFFFF on timeout.
 */
static uint32_t hda_codec_cmd(uint32_t verb)
{
    /* Wait until not busy */
    for (int i = 0; i < 1000; i++) {
        if (!(hda_read16(HDA_IRS) & HDA_IRS_ICB)) break;
        udelay(10);
    }
    if (hda_read16(HDA_IRS) & HDA_IRS_ICB) {
        serial_puts(SERIAL_COM1, "[hda] ICI busy timeout\n");
        return 0xFFFFFFFF;
    }

    /* Clear previous IRV */
    hda_write16(HDA_IRS, (uint16_t)HDA_IRS_IRV);

    /* Issue command */
    hda_write32(HDA_IC, verb);
    hda_write16(HDA_IRS, (uint16_t)(hda_read16(HDA_IRS) | HDA_IRS_ICB));

    /* Wait for response */
    for (int i = 0; i < 1000; i++) {
        uint16_t irs = hda_read16(HDA_IRS);
        if (irs & HDA_IRS_IRV)
            return hda_read32(HDA_IR);
        udelay(10);
    }

    serial_puts(SERIAL_COM1, "[hda] ICI response timeout\n");
    return 0xFFFFFFFF;
}

/* Convenience wrappers */
static uint32_t hda_get_param(uint8_t caddr, uint8_t nid, uint8_t param)
{
    return hda_codec_cmd(HDA_VERB(caddr, nid, HDA_VERB_GET_PARAM, param));
}

static void hda_set_power(uint8_t caddr, uint8_t nid, uint8_t state)
{
    hda_codec_cmd(HDA_VERB(caddr, nid, HDA_VERB_SET_POWER_STATE, state));
    udelay(100);
}

/* ── Controller reset ────────────────────────────────────────────────────── */
static bool hda_controller_reset(void)
{
    /* Assert reset: clear CRST */
    hda_write32(HDA_GCTL, hda_read32(HDA_GCTL) & ~(uint32_t)HDA_GCTL_CRST);
    udelay(100);

    /* Deassert reset: set CRST */
    hda_write32(HDA_GCTL, hda_read32(HDA_GCTL) | HDA_GCTL_CRST);

    /* Wait up to 10 ms for CRST to read back 1 (controller ready) */
    for (int i = 0; i < 100; i++) {
        if (hda_read32(HDA_GCTL) & HDA_GCTL_CRST) {
            /* Give codecs 521 µs to enumerate after reset de-assert */
            udelay(521);
            return true;
        }
        udelay(100);
    }
    serial_puts(SERIAL_COM1, "[hda] Controller reset timeout\n");
    return false;
}

/* ── Codec enumeration ───────────────────────────────────────────────────── */
static void hda_enumerate_codecs(void)
{
    uint16_t statests = hda_read16(HDA_STATESTS);
    /* Clear STATESTS by writing 1s to all set bits */
    hda_write16(HDA_STATESTS, statests);

    g_hda.num_codecs = 0;

    for (int i = 0; i < HDA_MAX_CODECS; i++) {
        if (!(statests & (uint16_t)(1u << i))) continue;

        hda_codec_t *c = &g_hda.codecs[g_hda.num_codecs];
        c->vendor_id   = hda_get_param((uint8_t)i, 0, HDA_PARAM_VENDOR_ID);
        c->revision_id = hda_get_param((uint8_t)i, 0, HDA_PARAM_REVISION_ID);
        c->afg_nid     = 0;
        c->present     = true;

        /* Find the Audio Function Group node */
        uint32_t node_cnt = hda_get_param((uint8_t)i, 0, HDA_PARAM_NODE_COUNT);
        uint8_t  start_nid = (uint8_t)((node_cnt >> 16) & 0xFF);
        uint8_t  total_nid = (uint8_t)( node_cnt        & 0xFF);

        for (uint8_t n = start_nid; n < start_nid + total_nid; n++) {
            uint32_t ftype = hda_get_param((uint8_t)i, n, HDA_PARAM_FUNC_TYPE);
            if ((ftype & 0xFF) == HDA_FUNC_AUDIO) {
                c->afg_nid = n;
                break;
            }
        }

        serial_puts(SERIAL_COM1, "[hda] Codec found (addr=");
        /* Simple hex nibble output */
        char tmp[3] = { (char)('0' + i), '\0', '\0' };
        serial_puts(SERIAL_COM1, tmp);
        serial_puts(SERIAL_COM1, " AFG=");
        tmp[0] = (char)('0' + (c->afg_nid >> 4));
        tmp[1] = (char)('0' + (c->afg_nid & 0xF));
        tmp[2] = '\0';
        serial_puts(SERIAL_COM1, tmp);
        serial_puts(SERIAL_COM1, ")\n");

        g_hda.num_codecs++;
    }
}

/* ── Output path configuration ───────────────────────────────────────────── */
/*
 * For each codec that has an AFG:
 *  1. Power up the AFG and all widgets.
 *  2. Walk all widgets looking for PIN widgets with output capability.
 *  3. Enable each output pin and optionally its EAPD booster.
 *  4. Find an OUTPUT converter connected to the pin (or upstream).
 *  5. Associate the converter with our stream and set 48 kHz / 16-bit stereo.
 *  6. Unmute and set max gain on the output amp.
 */
static uint8_t g_out_converter_nid = 0;   /* NID of the converter we'll use */
static uint8_t g_out_codec_addr    = 0;   /* codec address for the converter */

static void hda_configure_output(uint8_t caddr, const hda_codec_t *c)
{
    if (!c->afg_nid) return;

    /* Power up AFG */
    hda_set_power(caddr, c->afg_nid, HDA_PS_D0);

    /* Enumerate widgets inside the AFG */
    uint32_t node_cnt = hda_get_param(caddr, c->afg_nid, HDA_PARAM_NODE_COUNT);
    uint8_t  start    = (uint8_t)((node_cnt >> 16) & 0xFF);
    uint8_t  total    = (uint8_t)( node_cnt        & 0xFF);

    for (uint8_t nid = start; nid < start + total; nid++) {
        uint32_t wcap = hda_get_param(caddr, nid, HDA_PARAM_AUDIO_WIDGET_CAP);
        uint8_t  wtype = (uint8_t)((wcap >> 20) & 0xFu);

        /* Power up each widget */
        hda_set_power(caddr, nid, HDA_PS_D0);

        if (wtype == HDA_WID_PIN) {
            uint32_t pcap = hda_get_param(caddr, nid, HDA_PARAM_PIN_CAP);
            if (!(pcap & HDA_PINCAP_OUTPUT)) continue;

            /* Enable output (and headphone driver if supported) */
            uint8_t pinctl = HDA_PIN_OUT_ENABLE;
            if (pcap & HDA_PINCAP_HEADPHONE)
                pinctl |= HDA_PIN_HP_ENABLE;
            hda_codec_cmd(HDA_VERB(caddr, nid,
                                   HDA_VERB_SET_PIN_WIDGET_CTL, pinctl));

            /* Enable EAPD if supported */
            hda_codec_cmd(HDA_VERB(caddr, nid,
                                   HDA_VERB_SET_EAPD_BTL, HDA_EAPD_BTL));

            /* Get the first connection list entry — this is our converter */
            uint32_t conn = hda_codec_cmd(
                HDA_VERB(caddr, nid, HDA_VERB_GET_CONN_LIST, 0));
            uint8_t conv_nid = (uint8_t)(conn & 0x7Fu);
            if (conv_nid && !g_out_converter_nid) {
                g_out_converter_nid = conv_nid;
                g_out_codec_addr    = caddr;
            }
        }

        if (wtype == HDA_WID_OUTPUT && !g_out_converter_nid) {
            /* Keep a fallback in case we find no pin */
            g_out_converter_nid = nid;
            g_out_codec_addr    = caddr;
        }
    }

    /* Configure the chosen converter */
    if (g_out_converter_nid && g_out_codec_addr == caddr) {
        /* Associate with stream tag 1 and set PCM format */
        hda_codec_cmd(HDA_VERB(caddr, g_out_converter_nid,
                               HDA_VERB_SET_CHANNEL_STREAMID, 0x10));
        hda_codec_cmd(HDA_VERB4(caddr, g_out_converter_nid,
                                HDA_VERB_SET_CONVERTER_FMT,
                                HDA_FMT_48K_16BIT_STEREO));

        /* Unmute output amp at max volume on both channels */
        hda_codec_cmd(HDA_VERB4(caddr, g_out_converter_nid,
                                HDA_VERB_SET_AMP_GAIN,
                                HDA_AMP_OUT_UNMUTE_MAX));
    }
}

/* ── Output stream descriptor setup ─────────────────────────────────────── */
/*
 * Allocate the BDL and two PCM buffers (double-buffer scheme).
 * Configure SD[out_sd] for stream tag 1, 48 kHz, 16-bit stereo.
 */
static bool hda_stream_setup(void)
{
    uint8_t sd = g_hda.out_sd;

    /* Reset the stream descriptor */
    hda_write8(HDA_SD_CTL(sd), HDA_SD_CTL_SRST);
    for (int i = 0; i < 100; i++) {
        if (hda_read8(HDA_SD_CTL(sd)) & HDA_SD_CTL_SRST) break;
        udelay(100);
    }
    hda_write8(HDA_SD_CTL(sd), 0);
    for (int i = 0; i < 100; i++) {
        if (!(hda_read8(HDA_SD_CTL(sd)) & HDA_SD_CTL_SRST)) break;
        udelay(100);
    }

    /* Allocate BDL (2 entries × 16 bytes) */
    size_t bdl_pages = (sizeof(hda_bdl_entry_t) * HDA_BDL_ENTRIES
                        + PAGE_SIZE - 1) / PAGE_SIZE;
    g_hda.bdl_phys = pmm_alloc_pages(bdl_pages);
    if (!g_hda.bdl_phys) {
        serial_puts(SERIAL_COM1, "[hda] BDL alloc failed\n");
        return false;
    }
    g_hda.bdl = (hda_bdl_entry_t *)PHYS_TO_VIRT(g_hda.bdl_phys);
    memset(g_hda.bdl, 0, sizeof(hda_bdl_entry_t) * HDA_BDL_ENTRIES);

    /* Allocate two PCM buffers and fill the BDL */
    for (int i = 0; i < HDA_BDL_ENTRIES; i++) {
        size_t buf_pages = (HDA_BUF_BYTES + PAGE_SIZE - 1) / PAGE_SIZE;
        g_hda.pcm_buf_phys[i] = pmm_alloc_pages(buf_pages);
        if (!g_hda.pcm_buf_phys[i]) {
            serial_puts(SERIAL_COM1, "[hda] PCM buf alloc failed\n");
            return false;
        }
        g_hda.pcm_buf[i] = PHYS_TO_VIRT(g_hda.pcm_buf_phys[i]);
        memset(g_hda.pcm_buf[i], 0, HDA_BUF_BYTES);   /* silence */

        g_hda.bdl[i].addr   = g_hda.pcm_buf_phys[i];
        g_hda.bdl[i].length = HDA_BUF_BYTES;
        g_hda.bdl[i].ioc    = 1;   /* interrupt on each half */
    }

    /* Program stream descriptor */
    uint32_t total_bytes = HDA_BUF_BYTES * HDA_BDL_ENTRIES;
    hda_write32(HDA_SD_CBL(sd), total_bytes);
    hda_write16(HDA_SD_LVI(sd), (uint16_t)(HDA_BDL_ENTRIES - 1));
    hda_write16(HDA_SD_FMT(sd), HDA_FMT_48K_16BIT_STEREO);
    hda_write32(HDA_SD_BDPL(sd), (uint32_t)(g_hda.bdl_phys & 0xFFFFFFFFu));
    hda_write32(HDA_SD_BDPU(sd), (uint32_t)(g_hda.bdl_phys >> 32));

    /* Set stream tag 1 in the top byte of SD CTL [23:20] */
    uint32_t ctl = hda_read32(HDA_SD_CTL(sd));
    ctl = (ctl & ~(0xFu << 20)) | (1u << 20);   /* tag = 1 */
    hda_write32(HDA_SD_CTL(sd), ctl);

    return true;
}

/* ── Public API ───────────────────────────────────────────────────────────── */
bool hda_start_stream(void)
{
    if (!g_hda_found) return false;

    uint8_t sd = g_hda.out_sd;
    uint32_t ctl = hda_read32(HDA_SD_CTL(sd));
    ctl |= HDA_SD_CTL_RUN | HDA_SD_CTL_IOCE;
    hda_write32(HDA_SD_CTL(sd), ctl);
    g_hda.stream_running = true;

    serial_puts(SERIAL_COM1, "[hda] Output stream started\n");
    return true;
}

void hda_stop_stream(void)
{
    if (!g_hda_found) return;

    uint8_t sd = g_hda.out_sd;
    uint32_t ctl = hda_read32(HDA_SD_CTL(sd));
    ctl &= ~(uint32_t)(HDA_SD_CTL_RUN | HDA_SD_CTL_IOCE);
    hda_write32(HDA_SD_CTL(sd), ctl);
    g_hda.stream_running = false;

    serial_puts(SERIAL_COM1, "[hda] Output stream stopped\n");
}

void hda_init(void)
{
    serial_puts(SERIAL_COM1, "[hda] Scanning PCI for Intel HDA controller\n");

    /* Try explicit device-ID probe first */
    pci_dev_t *pdev = NULL;
    for (size_t k = 0; k < sizeof(hda_device_ids)/sizeof(hda_device_ids[0]); k++) {
        pdev = pci_find_device(HDA_VENDOR_INTEL, hda_device_ids[k]);
        if (pdev) break;
    }

    /* Fall back to PCI class scan (catches OEM/future IDs) */
    if (!pdev)
        pdev = pci_find_device_by_class(PCI_CLASS_MM, PCI_SUBCLASS_HDA);

    if (!pdev) {
        serial_puts(SERIAL_COM1, "[hda] Intel HDA controller not found\n");
        return;
    }

    serial_puts(SERIAL_COM1, "[hda] Intel HDA controller found\n");
    pci_enable_device(pdev);

    g_hda.pdev = pdev;
    g_hda.mmio = pci_map_bar(pdev, 0);
    if (!g_hda.mmio) {
        serial_puts(SERIAL_COM1, "[hda] Failed to map BAR0\n");
        return;
    }

    /* Parse GCAP for stream counts */
    uint16_t gcap = hda_read16(HDA_GCAP);
    g_hda.num_iss = (uint8_t)((gcap >> 8) & 0xFu);
    g_hda.num_oss = (uint8_t)((gcap >> 12) & 0xFu);
    if (g_hda.num_oss == 0) g_hda.num_oss = 1;   /* assume at least one */
    g_hda.out_sd  = g_hda.num_iss;               /* first output stream */

    if (!hda_controller_reset()) {
        serial_puts(SERIAL_COM1, "[hda] Controller reset failed\n");
        return;
    }

    /* Accept unsolicited events */
    hda_write32(HDA_GCTL, hda_read32(HDA_GCTL) | HDA_GCTL_UNSOL);

    hda_enumerate_codecs();

    if (g_hda.num_codecs == 0) {
        serial_puts(SERIAL_COM1, "[hda] No codecs found\n");
        return;
    }

    /* Configure output path for each codec that has an AFG */
    for (uint8_t i = 0; i < g_hda.num_codecs; i++) {
        if (g_hda.codecs[i].present && g_hda.codecs[i].afg_nid)
            hda_configure_output(i, &g_hda.codecs[i]);
    }

    if (!hda_stream_setup()) {
        serial_puts(SERIAL_COM1, "[hda] Stream setup failed\n");
        return;
    }

    g_hda_found = true;

    /* Start playback immediately (silence until user fills the buffers) */
    hda_start_stream();

    /* Register with /zirv/audio/output0 */
    device_desc_t *desc = (device_desc_t *)kzalloc(sizeof(device_desc_t), 0);
    if (desc) {
        desc->bus_class   = DEV_CLASS_AUDIO_OUTPUT;
        desc->media_class = DEV_CLASS_AUDIO_OUTPUT;
        desc->present     = true;
        desc->driver_data = &g_hda;
        const char *model = "Intel High Definition Audio";
        for (int i = 0; model[i] && i < 63; i++) desc->model[i] = model[i];
        vfs_register_device(DEV_CLASS_AUDIO_OUTPUT, DEV_CLASS_AUDIO_OUTPUT,
                            0, desc);
    }

    serial_puts(SERIAL_COM1,
        "[hda] Audio ready → /zirv/audio/output0 (48 kHz / 16-bit stereo)\n");
}
