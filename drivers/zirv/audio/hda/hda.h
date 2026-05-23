/* drivers/zirv/audio/hda/hda.h
 * Zirvium Kernel — Intel High Definition Audio (HDA) driver
 *
 * Covers Intel cAVS / snd_hda_intel PCH audio controllers paired with
 * Intel integrated graphics (i915).  Common on Kaby Lake through Meteor
 * Lake laptop and desktop platforms.
 *
 * Supported PCI IDs (Intel PCH HDA, class 0x04/0x03):
 *   0x9D71  Kaby Lake-LP PCH
 *   0xA171  Kaby Lake-H  PCH
 *   0x9DC8  Coffee Lake-LP PCH
 *   0xA348  Coffee Lake-H  PCH
 *   0x34C8  Ice Lake-LP PCH
 *   0x43C8  Tiger Lake   PCH
 *   0x7AD0  Alder Lake    PCH
 *   0x51C8  Alder Lake-P  PCH
 *   0x54C8  Alder Lake-N  PCH
 *   0xA828  Raptor Lake   PCH
 *   0x7E28  Meteor Lake   PCH
 *
 * Linux reference: sound/pci/hda/hda_intel.c  (GPL-2.0, Intel Corp.)
 * Ported to Zirvium MOSIX APIs.
 *
 * The driver uses the Immediate Command Interface (ICI/IR/IRS) for all
 * codec communication, avoiding CORB/RIRB DMA ring setup.
 *
 * Initialisation sequence:
 *  1. Reset the controller (GCTL.CRST)
 *  2. Enumerate present codecs via STATESTS
 *  3. For each codec: read vendor/device ID
 *  4. Find the Audio Function Group (AFG) node
 *  5. Power up the AFG
 *  6. Enumerate audio widgets
 *  7. Configure output pins (headphone + speaker) and converters
 *  8. Set up one output stream descriptor (PCM, 48 kHz, 16-bit stereo)
 *  9. Register under /zirv/audio/output0
 */
#ifndef ZIRVIUM_DRIVERS_AUDIO_HDA_H
#define ZIRVIUM_DRIVERS_AUDIO_HDA_H

#include "drivers/pci/pci.h"
#include <stdint.h>
#include <stdbool.h>

/* ── PCI identification ──────────────────────────────────────────────────── */
#define HDA_VENDOR_INTEL    0x8086
#define PCI_CLASS_MM        0x04   /* Multimedia */
#define PCI_SUBCLASS_HDA    0x03   /* HD Audio   */

/* ── Controller MMIO register offsets (Intel HDA spec §3.3) ─────────────── */
#define HDA_GCAP         0x00   /* Global Capabilities        (16-bit) */
#define HDA_VMIN         0x02   /* Minor Version              ( 8-bit) */
#define HDA_VMAJ         0x03   /* Major Version              ( 8-bit) */
#define HDA_GCTL         0x08   /* Global Control             (32-bit) */
#define  HDA_GCTL_CRST     (1u << 0)   /* Controller Reset          */
#define  HDA_GCTL_FCNTRL   (1u << 1)   /* Flush Control             */
#define  HDA_GCTL_UNSOL    (1u << 8)   /* Accept Unsolicited Events */
#define HDA_WAKEEN       0x0C   /* Wake Enable                (16-bit) */
#define HDA_STATESTS     0x0E   /* State Change Status        (16-bit) */
#define HDA_INTCTL       0x20   /* Interrupt Control          (32-bit) */
#define  HDA_INTCTL_GIE    (1u << 31)
#define  HDA_INTCTL_CIE    (1u << 30)
#define HDA_INTSTS       0x24   /* Interrupt Status           (32-bit) */
#define HDA_WALCLK       0x30   /* Wall Clock Counter         (32-bit) */

/* Immediate Command Interface */
#define HDA_IC           0x60   /* Immediate Command output   (32-bit) */
#define HDA_IR           0x64   /* Immediate Response input   (32-bit) */
#define HDA_IRS          0x68   /* Immediate Response Status  (16-bit) */
#define  HDA_IRS_ICB       (1u << 0)   /* Immediate Command Busy    */
#define  HDA_IRS_IRV       (1u << 1)   /* Immediate Response Valid  */

/* Stream Descriptor n base (n = 0..30; output streams after input streams) */
#define HDA_SD_BASE(n)   (0x80u + (uint32_t)(n) * 0x20u)
#define HDA_SD_CTL(n)    (HDA_SD_BASE(n) + 0x00u)  /* Stream Control  (24-bit) */
#define  HDA_SD_CTL_RUN    (1u << 1)
#define  HDA_SD_CTL_IOCE   (1u << 2)
#define  HDA_SD_CTL_FEIE   (1u << 3)
#define  HDA_SD_CTL_DEIE   (1u << 4)
#define  HDA_SD_CTL_SRST   (1u << 0)
#define HDA_SD_STS(n)    (HDA_SD_BASE(n) + 0x03u)  /* Stream Status    ( 8-bit) */
#define  HDA_SD_STS_BCIS   (1u << 2)   /* Buffer Completion Interrupt */
#define HDA_SD_LPIB(n)   (HDA_SD_BASE(n) + 0x04u)  /* Link Pos in Buf (32-bit) */
#define HDA_SD_CBL(n)    (HDA_SD_BASE(n) + 0x08u)  /* Cyclic Buf Len  (32-bit) */
#define HDA_SD_LVI(n)    (HDA_SD_BASE(n) + 0x0Cu)  /* Last Valid Index (16-bit) */
#define HDA_SD_FMT(n)    (HDA_SD_BASE(n) + 0x12u)  /* Format          (16-bit) */
#define HDA_SD_BDPL(n)   (HDA_SD_BASE(n) + 0x18u)  /* BDL Addr Low    (32-bit) */
#define HDA_SD_BDPU(n)   (HDA_SD_BASE(n) + 0x1Cu)  /* BDL Addr High   (32-bit) */

/* ── HDA stream format word (HDA spec §3.7.1) ────────────────────────────── */
/* [14]    = BASE (0=48kHz, 1=44.1kHz family)
 * [13:11] = MULT (0=×1, 1=×2, 2=×3, 3=×4)
 * [10:8]  = DIV  (0÷1 … 7÷8)
 * [6:4]   = BITS (0=8-bit, 1=16-bit, 2=20-bit, 3=24-bit, 4=32-bit)
 * [3:0]   = CHAN minus 1 (0=mono, 1=stereo, …)                        */
#define HDA_FMT_48K_16BIT_STEREO  0x0011u   /* 48 kHz, ×1, ÷1, 16-bit, 2ch */
#define HDA_FMT_44K_16BIT_STEREO  0x4011u   /* 44.1 kHz base, 16-bit, 2ch  */

/* ── Codec verb format ───────────────────────────────────────────────────── */
/* [31:28]=codec_addr  [27:20]=node_id  [19:8]=verb12  [7:0]=payload8  */
#define HDA_VERB(caddr, nid, verb, payload) \
    (((uint32_t)(caddr) << 28) | ((uint32_t)(nid) << 20) | \
     ((uint32_t)(verb)  <<  8) | ((uint32_t)(payload) & 0xFFu))

/* 4-bit verbs (verb in bits [19:16], payload in [15:0]) */
#define HDA_VERB4(caddr, nid, verb4, payload16) \
    (((uint32_t)(caddr) << 28) | ((uint32_t)(nid) << 20) | \
     ((uint32_t)(verb4) << 16) | ((uint32_t)(payload16) & 0xFFFFu))

/* Verb IDs (12-bit) */
#define HDA_VERB_GET_PARAM           0xF00
#define HDA_VERB_GET_CONN_SEL        0xF01
#define HDA_VERB_GET_CONN_LIST       0xF02
#define HDA_VERB_GET_PROC_STATE      0xF03
#define HDA_VERB_GET_SDI_SELECT      0xF04
#define HDA_VERB_GET_POWER_STATE     0xF05
#define HDA_VERB_GET_CONVERTER_FMT   0xA00   /* 4-bit verb */
#define HDA_VERB_GET_AMP_GAIN        0xB00   /* 4-bit verb */
#define HDA_VERB_GET_CONFIG_DEFAULT  0xF1C
#define HDA_VERB_SET_CONN_SEL        0x701
#define HDA_VERB_SET_POWER_STATE     0x705
#define HDA_VERB_SET_CHANNEL_STREAMID 0x706
#define HDA_VERB_SET_PIN_WIDGET_CTL  0x707
#define HDA_VERB_SET_EAPD_BTL        0x70C
#define HDA_VERB_SET_CONVERTER_FMT   0x200   /* 4-bit verb */
#define HDA_VERB_SET_AMP_GAIN        0x300   /* 4-bit verb, output amp */

/* Parameter IDs (used with GET_PARAM) */
#define HDA_PARAM_VENDOR_ID          0x00
#define HDA_PARAM_REVISION_ID        0x02
#define HDA_PARAM_NODE_COUNT         0x04
#define HDA_PARAM_FUNC_TYPE          0x05
#define HDA_PARAM_AUDIO_WIDGET_CAP   0x09
#define HDA_PARAM_PCM_SIZES          0x0A
#define HDA_PARAM_PIN_CAP            0x0C
#define HDA_PARAM_OUTPUT_AMP_CAP     0x12

/* Functional group types */
#define HDA_FUNC_AUDIO               0x01

/* Audio widget types (AUDIO_WIDGET_CAP bits [23:20]) */
#define HDA_WID_OUTPUT               0x00
#define HDA_WID_INPUT                0x01
#define HDA_WID_MIXER                0x02
#define HDA_WID_SELECTOR             0x03
#define HDA_WID_PIN                  0x04
#define HDA_WID_POWER                0x05
#define HDA_WID_VOLUME_KNOB          0x06
#define HDA_WID_BEEP                 0x07

/* PIN_CAP bits */
#define HDA_PINCAP_OUTPUT            (1u << 4)
#define HDA_PINCAP_HEADPHONE         (1u << 7)

/* SET_PIN_WIDGET_CTL bits */
#define HDA_PIN_OUT_ENABLE           (1u << 6)
#define HDA_PIN_HP_ENABLE            (1u << 7)

/* Power states */
#define HDA_PS_D0                    0x00   /* fully on */
#define HDA_PS_D3                    0x03   /* off      */

/* SET_EAPD_BTL bits */
#define HDA_EAPD_BTL                 (1u << 1)

/* SET_AMP_GAIN payload: output amp, left+right channels, max volume, unmute */
#define HDA_AMP_OUT_UNMUTE_MAX       0xB080u  /* B=out,L+R; 80=max gain+unmute */

/* ── BDL (Buffer Descriptor List) entry ─────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint64_t addr;     /* physical address of buffer segment */
    uint32_t length;   /* byte length of segment */
    uint32_t ioc;      /* interrupt-on-completion flag (bit 0) */
} hda_bdl_entry_t;

#define HDA_BDL_ENTRIES   8            /* ring-buffer depth: 8 × half-second each */
#define HDA_BUF_FRAMES    2048         /* PCM frames per BDL segment           */
#define HDA_BUF_BYTES     (HDA_BUF_FRAMES * 4)  /* 16-bit stereo = 4 bytes/frame */

/* ── Per-codec info ──────────────────────────────────────────────────────── */
#define HDA_MAX_CODECS    15
typedef struct {
    uint32_t vendor_id;    /* GET_PARAM(VENDOR_ID)   */
    uint32_t revision_id;  /* GET_PARAM(REVISION_ID) */
    uint8_t  afg_nid;      /* Audio Function Group node ID */
    bool     present;
} hda_codec_t;

/* ── Driver state ────────────────────────────────────────────────────────── */
typedef struct {
    pci_dev_t    *pdev;
    void         *mmio;             /* BAR0 virtual address */
    uint8_t       num_iss;          /* input streams from GCAP */
    uint8_t       num_oss;          /* output streams from GCAP */
    uint8_t       out_sd;           /* absolute index of first output SD */
    hda_codec_t   codecs[HDA_MAX_CODECS];
    uint8_t       num_codecs;

    /* Output stream PCM buffers */
    hda_bdl_entry_t *bdl;           /* virtual address of the BDL  */
    uint64_t          bdl_phys;
    void             *pcm_buf[HDA_BDL_ENTRIES];
    uint64_t          pcm_buf_phys[HDA_BDL_ENTRIES];
    bool              stream_running;
    volatile int      write_idx;    /* next buffer to fill (ring) */
    int               irq;
} hda_priv_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * hda_init - detect an Intel HDA controller on the PCI bus and initialise it.
 * Enumerates codecs, configures the default audio output path, and registers
 * the device as /zirv/audio/output0.
 */
void hda_init(void);

/**
 * hda_write_pcm - copy PCM frames into the next available ring-buffer slot.
 * @buf:     interleaved 16-bit stereo PCM data (4 bytes per frame)
 * @frames:  number of stereo frames to copy (max HDA_BUF_FRAMES)
 * Returns the number of frames actually written (0 if no slot free).
 */
uint32_t hda_write_pcm(const void *buf, uint32_t frames);

/**
 * hda_start_stream - start the output stream (sends PCM silence initially).
 */
void hda_start_stream(void);

/**
 * hda_stop_stream - stop the output stream.
 */
void hda_stop_stream(void);

#endif /* ZIRVIUM_DRIVERS_AUDIO_HDA_H */
