/*
 * Intel HD Audio Driver
 * Zirvium OS - High Definition Audio Controller
 */

#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>

#define INTEL_HDA_VENDOR 0x8086

/* HDA Register offsets */
#define HDA_REG_GCAP     0x00  /* Global Capabilities */
#define HDA_REG_VMIN     0x02  /* Minor Version */
#define HDA_REG_VMAJ     0x03  /* Major Version */
#define HDA_REG_OUTPAY   0x04  /* Output Payload Capability */
#define HDA_REG_INPAY    0x06  /* Input Payload Capability */
#define HDA_REG_GCTL     0x08  /* Global Control */
#define HDA_REG_WAKEEN   0x0C  /* Wake Enable */
#define HDA_REG_STATESTS 0x0E  /* State Change Status */
#define HDA_REG_GSTS     0x10  /* Global Status */
#define HDA_REG_INTCTL   0x20  /* Interrupt Control */
#define HDA_REG_INTSTS   0x24  /* Interrupt Status */
#define HDA_REG_WALCLK   0x30  /* Wall Clock Counter */
#define HDA_REG_SSYNC    0x38  /* Stream Synchronization */
#define HDA_REG_CORBLBASE 0x40 /* CORB Lower Base Address */
#define HDA_REG_CORBUBASE 0x44 /* CORB Upper Base Address */
#define HDA_REG_CORBWP   0x48  /* CORB Write Pointer */
#define HDA_REG_CORBRP   0x4A  /* CORB Read Pointer */
#define HDA_REG_CORBCTL  0x4C  /* CORB Control */
#define HDA_REG_CORBSTS  0x4D  /* CORB Status */
#define HDA_REG_CORBSIZE 0x4E  /* CORB Size */
#define HDA_REG_RIRBLBASE 0x50 /* RIRB Lower Base Address */
#define HDA_REG_RIRBUBASE 0x54 /* RIRB Upper Base Address */
#define HDA_REG_RIRBWP   0x58  /* RIRB Write Pointer */
#define HDA_REG_RINTCNT  0x5A  /* Response Interrupt Count */
#define HDA_REG_RIRBCTL  0x5C  /* RIRB Control */
#define HDA_REG_RIRBSTS  0x5D  /* RIRB Status */
#define HDA_REG_RIRBSIZE 0x5E  /* RIRB Size */

/* Stream Descriptor Registers (SDn) */
#define HDA_REG_SD0_CTL  0x80
#define HDA_REG_SD0_STS  0x83
#define HDA_REG_SD0_LPIB 0x84
#define HDA_REG_SD0_CBL  0x88
#define HDA_REG_SD0_LVI  0x8C
#define HDA_REG_SD0_FIFOS 0x90
#define HDA_REG_SD0_FMT  0x92
#define HDA_REG_SD0_BDPL 0x98
#define HDA_REG_SD0_BDPU 0x9C

/* Codec Commands */
#define HDA_VERB_GET_PARAM          0xF00
#define HDA_VERB_GET_CONN_SELECT    0xF01
#define HDA_VERB_SET_CONN_SELECT    0x701
#define HDA_VERB_GET_STREAM_FORMAT  0xA00
#define HDA_VERB_SET_STREAM_FORMAT  0x200
#define HDA_VERB_GET_AMP_GAIN       0xB00
#define HDA_VERB_SET_AMP_GAIN       0x300
#define HDA_VERB_GET_PIN_CTRL       0xF07
#define HDA_VERB_SET_PIN_CTRL       0x707
#define HDA_VERB_GET_POWER_STATE    0xF05
#define HDA_VERB_SET_POWER_STATE    0x705

/* Audio Format */
#define HDA_FMT_48KHZ  (0 << 0)
#define HDA_FMT_44KHZ  (1 << 0)
#define HDA_FMT_16BIT  (0 << 4)
#define HDA_FMT_20BIT  (1 << 4)
#define HDA_FMT_24BIT  (2 << 4)
#define HDA_FMT_32BIT  (3 << 4)
#define HDA_FMT_MONO   (0 << 8)
#define HDA_FMT_STEREO (1 << 8)

typedef struct {
    uint64_t addr;
    uint32_t length;
    uint32_t ioc;
} __attribute__((packed)) hda_bdl_entry_t;

typedef struct {
    uint32_t mmio_base;
    uint8_t vmaj;
    uint8_t vmin;
    
    /* Capabilities */
    uint8_t num_input_streams;
    uint8_t num_output_streams;
    uint8_t num_bidirectional_streams;
    uint16_t num_codecs;
    
    /* CORB/RIRB (Command/Response buffers) */
    uint32_t *corb_buffer;
    uint64_t *rirb_buffer;
    uint8_t corb_wp;
    uint8_t rirb_rp;
    
    /* Audio streams */
    void *output_buffer;
    void *input_buffer;
    uint32_t sample_rate;
    uint16_t format;
    
    /* Volume control */
    uint8_t master_volume;
    uint8_t master_mute;
    
    /* Statistics */
    uint32_t samples_played;
    uint32_t samples_recorded;
} hda_controller_t;

static hda_controller_t hda_ctrl;

static inline uint32_t hda_read32(uint32_t reg) {
    return *((volatile uint32_t*)(hda_ctrl.mmio_base + reg));
}

static inline void hda_write32(uint32_t reg, uint32_t value) {
    *((volatile uint32_t*)(hda_ctrl.mmio_base + reg)) = value;
}

static inline uint16_t hda_read16(uint32_t reg) {
    return *((volatile uint16_t*)(hda_ctrl.mmio_base + reg));
}

static inline void hda_write16(uint32_t reg, uint16_t value) {
    *((volatile uint16_t*)(hda_ctrl.mmio_base + reg)) = value;
}

static inline uint8_t hda_read8(uint32_t reg) {
    return *((volatile uint8_t*)(hda_ctrl.mmio_base + reg));
}

static inline void hda_write8(uint32_t reg, uint8_t value) {
    *((volatile uint8_t*)(hda_ctrl.mmio_base + reg)) = value;
}

static int hda_reset_controller(void) {
    /* Clear GCTL */
    hda_write32(HDA_REG_GCTL, 0);
    
    /* Wait for reset */
    for (int i = 0; i < 1000; i++) {
        if ((hda_read32(HDA_REG_GCTL) & 1) == 0)
            break;
    }
    
    /* Set GCTL */
    hda_write32(HDA_REG_GCTL, 1);
    
    /* Wait for ready */
    for (int i = 0; i < 1000; i++) {
        if (hda_read32(HDA_REG_GCTL) & 1)
            return 0;
    }
    
    return -1;
}

static uint64_t hda_send_command(uint8_t codec, uint8_t nid, 
                                 uint16_t verb, uint8_t param) {
    uint32_t cmd = ((uint32_t)codec << 28) | ((uint32_t)nid << 20) | 
                   ((uint32_t)verb << 8) | param;
    
    /* Write to CORB */
    uint8_t wp = hda_read16(HDA_REG_CORBWP) & 0xFF;
    hda_ctrl.corb_buffer[wp] = cmd;
    hda_write16(HDA_REG_CORBWP, (wp + 1) & 0xFF);
    
    /* Wait for response in RIRB */
    for (int i = 0; i < 1000; i++) {
        uint8_t new_wp = hda_read16(HDA_REG_RIRBWP) & 0xFF;
        if (new_wp != hda_ctrl.rirb_rp) {
            uint64_t resp = hda_ctrl.rirb_buffer[hda_ctrl.rirb_rp];
            hda_ctrl.rirb_rp = (hda_ctrl.rirb_rp + 1) & 0xFF;
            return resp;
        }
    }
    
    return 0;
}

int intel_hda_init(void) {
    kprintf("INTEL-HDA: Intel High Definition Audio driver\n");
    kprintf("INTEL-HDA: Vendor ID: 0x%x\n", INTEL_HDA_VENDOR);
    
    /* TODO: PCI device discovery */
    hda_ctrl.mmio_base = 0xFEB00000;
    
    /* Read version */
    hda_ctrl.vmaj = hda_read8(HDA_REG_VMAJ);
    hda_ctrl.vmin = hda_read8(HDA_REG_VMIN);
    kprintf("INTEL-HDA: Version %d.%d\n", hda_ctrl.vmaj, hda_ctrl.vmin);
    
    /* Reset controller */
    if (hda_reset_controller() != 0) {
        kprintf("INTEL-HDA: Failed to reset controller\n");
        return -1;
    }
    
    /* Read capabilities */
    uint16_t gcap = hda_read16(HDA_REG_GCAP);
    hda_ctrl.num_output_streams = (gcap >> 12) & 0xF;
    hda_ctrl.num_input_streams = (gcap >> 8) & 0xF;
    hda_ctrl.num_bidirectional_streams = (gcap >> 3) & 0x1F;
    
    kprintf("INTEL-HDA: Streams - Out: %d, In: %d, Bidir: %d\n",
            hda_ctrl.num_output_streams,
            hda_ctrl.num_input_streams,
            hda_ctrl.num_bidirectional_streams);
    
    /* Detect codecs */
    uint16_t statests = hda_read16(HDA_REG_STATESTS);
    hda_ctrl.num_codecs = 0;
    for (int i = 0; i < 15; i++) {
        if (statests & (1 << i)) {
            hda_ctrl.num_codecs++;
            kprintf("INTEL-HDA: Codec %d detected\n", i);
        }
    }
    
    /* Allocate CORB/RIRB */
    hda_ctrl.corb_buffer = (uint32_t*)kmalloc(256 * sizeof(uint32_t));
    hda_ctrl.rirb_buffer = (uint64_t*)kmalloc(256 * sizeof(uint64_t));
    
    /* Setup CORB */
    hda_write32(HDA_REG_CORBLBASE, (uint32_t)(uintptr_t)hda_ctrl.corb_buffer);
    hda_write32(HDA_REG_CORBUBASE, 0);
    hda_write8(HDA_REG_CORBSIZE, 2); /* 256 entries */
    hda_write16(HDA_REG_CORBRP, 0x8000); /* Reset read pointer */
    hda_write16(HDA_REG_CORBWP, 0);
    hda_write8(HDA_REG_CORBCTL, 2); /* Enable CORB */
    
    /* Setup RIRB */
    hda_write32(HDA_REG_RIRBLBASE, (uint32_t)(uintptr_t)hda_ctrl.rirb_buffer);
    hda_write32(HDA_REG_RIRBUBASE, 0);
    hda_write8(HDA_REG_RIRBSIZE, 2); /* 256 entries */
    hda_write16(HDA_REG_RIRBWP, 0x8000); /* Reset write pointer */
    hda_write8(HDA_REG_RIRBCTL, 2); /* Enable RIRB */
    
    /* Default audio format: 48kHz, 16-bit, Stereo */
    hda_ctrl.sample_rate = 48000;
    hda_ctrl.format = HDA_FMT_48KHZ | HDA_FMT_16BIT | HDA_FMT_STEREO;
    hda_ctrl.master_volume = 75;
    hda_ctrl.master_mute = 0;
    
    kprintf("INTEL-HDA: Initialized successfully\n");
    kprintf("INTEL-HDA: Format: 48kHz, 16-bit, Stereo\n");
    
    return 0;
}

int intel_hda_set_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    
    hda_ctrl.master_volume = volume;
    kprintf("INTEL-HDA: Volume set to %d%%\n", volume);
    
    /* TODO: Send codec command to set volume */
    
    return 0;
}

int intel_hda_set_mute(uint8_t mute) {
    hda_ctrl.master_mute = mute ? 1 : 0;
    kprintf("INTEL-HDA: Audio %s\n", mute ? "muted" : "unmuted");
    
    /* TODO: Send codec command to mute/unmute */
    
    return 0;
}

int intel_hda_play_buffer(const void *buffer, size_t size) {
    (void)buffer; (void)size;
    
    /* TODO: Setup stream descriptor and play audio */
    hda_ctrl.samples_played += size / 4; /* Assuming 16-bit stereo */
    
    return 0;
}

void intel_hda_cleanup(void) {
    /* Stop all streams */
    hda_write32(HDA_REG_GCTL, 0);
    
    kprintf("INTEL-HDA: Driver unloaded (Played: %d samples)\n",
            hda_ctrl.samples_played);
}
