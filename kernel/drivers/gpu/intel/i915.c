/*
 * Intel i915 Graphics Driver
 * Zirvium OS - HD Graphics, Iris, Xe support
 */

#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>

#define INTEL_VENDOR_ID 0x8086

/* Device IDs - HD Graphics */
#define INTEL_HD2000  0x0102
#define INTEL_HD3000  0x0116
#define INTEL_HD4000  0x0166
#define INTEL_HD4600  0x0412
#define INTEL_HD5000  0x0A26
#define INTEL_HD6000  0x1626

/* Iris Graphics */
#define INTEL_IRIS_5100 0x0A2E
#define INTEL_IRIS_6100 0x162B
#define INTEL_IRIS_PRO_580 0x193B

/* Xe Graphics */
#define INTEL_XE_DG1   0x4905
#define INTEL_XE_HP    0x0BD0
#define INTEL_ARC_A770 0x56A0
#define INTEL_ARC_A750 0x56A1

/* Register offsets */
#define INTEL_REG_DISPLAY_BASE    0x70000
#define INTEL_REG_RENDER_BASE     0x02000
#define INTEL_REG_VECS_BASE       0x1A000
#define INTEL_REG_BLT_BASE        0x22000

/* Display registers */
#define INTEL_REG_PIPEA_CONF      0x70008
#define INTEL_REG_PIPEB_CONF      0x71008
#define INTEL_REG_PIPEC_CONF      0x72008
#define INTEL_REG_DPLL_A          0x06014
#define INTEL_REG_DPLL_B          0x06018

/* GPU Power */
#define INTEL_REG_FORCEWAKE       0x0A18C
#define INTEL_REG_RC_CONTROL      0x0A090

/* Memory */
#define INTEL_REG_GTT_BASE        0x100000
#define INTEL_REG_STOLEN_BASE     0x5C

typedef struct {
    uint32_t mmio_base;
    uint32_t gtt_base;
    uint16_t device_id;
    const char *gpu_name;
    
    /* GPU Generation */
    uint8_t generation;         /* 2-12+ */
    
    /* Capabilities */
    uint32_t gpu_freq_mhz;
    uint32_t execution_units;   /* EUs */
    uint32_t memory_size_mb;
    uint8_t num_displays;
    
    /* Display configuration */
    uint32_t width;
    uint32_t height;
    uint32_t refresh_rate;
    uint64_t framebuffer_addr;
    
    /* Features */
    uint8_t has_edp;           /* Embedded DisplayPort */
    uint8_t has_hdmi;
    uint8_t has_dp;            /* DisplayPort */
    uint8_t has_quicksync;     /* Hardware video encode/decode */
    uint8_t has_xe_hpg;        /* Xe-HPG architecture (Arc) */
    
    /* Power management */
    uint8_t power_state;
    uint32_t temperature;
    
    /* Statistics */
    uint32_t frames_rendered;
    uint32_t gpu_busy_percent;
} intel_gpu_t;

static intel_gpu_t intel_gpu;

static inline uint32_t intel_read32(uint32_t reg) {
    return *((volatile uint32_t*)(intel_gpu.mmio_base + reg));
}

static inline void intel_write32(uint32_t reg, uint32_t value) {
    *((volatile uint32_t*)(intel_gpu.mmio_base + reg)) = value;
}

static const char* intel_get_gpu_name(uint16_t device_id) {
    switch (device_id) {
        case INTEL_HD2000: return "Intel HD Graphics 2000";
        case INTEL_HD3000: return "Intel HD Graphics 3000";
        case INTEL_HD4000: return "Intel HD Graphics 4000";
        case INTEL_HD4600: return "Intel HD Graphics 4600";
        case INTEL_HD5000: return "Intel HD Graphics 5000";
        case INTEL_HD6000: return "Intel HD Graphics 6000";
        case INTEL_IRIS_5100: return "Intel Iris Graphics 5100";
        case INTEL_IRIS_6100: return "Intel Iris Graphics 6100";
        case INTEL_IRIS_PRO_580: return "Intel Iris Pro Graphics 580";
        case INTEL_XE_DG1: return "Intel Xe DG1";
        case INTEL_XE_HP: return "Intel Xe-HP";
        case INTEL_ARC_A770: return "Intel Arc A770";
        case INTEL_ARC_A750: return "Intel Arc A750";
        default: return "Intel Graphics";
    }
}

static uint8_t intel_get_generation(uint16_t device_id) {
    if (device_id >= 0x0100 && device_id < 0x0200) return 6;  /* Sandy Bridge */
    if (device_id >= 0x0150 && device_id < 0x0200) return 7;  /* Ivy Bridge */
    if (device_id >= 0x0400 && device_id < 0x0500) return 7;  /* Haswell */
    if (device_id >= 0x0A00 && device_id < 0x0B00) return 7;  /* Haswell ULT */
    if (device_id >= 0x1600 && device_id < 0x1700) return 8;  /* Broadwell */
    if (device_id >= 0x1900 && device_id < 0x2000) return 9;  /* Skylake */
    if (device_id >= 0x5900 && device_id < 0x5A00) return 9;  /* Kaby Lake */
    if (device_id >= 0x3E00 && device_id < 0x3F00) return 9;  /* Coffee Lake */
    if (device_id >= 0x8A00 && device_id < 0x8B00) return 11; /* Ice Lake */
    if (device_id >= 0x9A00 && device_id < 0x9B00) return 12; /* Tiger Lake */
    if (device_id >= 0x4600 && device_id < 0x4800) return 12; /* DG1 */
    if (device_id >= 0x5600 && device_id < 0x5700) return 12; /* Arc Alchemist */
    return 6;
}

int intel_i915_init(void) {
    kprintf("INTEL-i915: Intel Graphics driver\n");
    kprintf("INTEL-i915: Vendor ID: 0x%x\n", INTEL_VENDOR_ID);
    
    /* TODO: PCI device discovery */
    intel_gpu.mmio_base = 0xF0000000;
    intel_gpu.device_id = INTEL_HD4600;
    intel_gpu.gpu_name = intel_get_gpu_name(intel_gpu.device_id);
    intel_gpu.generation = intel_get_generation(intel_gpu.device_id);
    
    kprintf("INTEL-i915: GPU: %s\n", intel_gpu.gpu_name);
    kprintf("INTEL-i915: Generation: %d\n", intel_gpu.generation);
    
    /* GPU specifications (example: HD 4600) */
    intel_gpu.execution_units = 20;
    intel_gpu.gpu_freq_mhz = 1200;
    intel_gpu.memory_size_mb = 2048; /* Shared system memory */
    intel_gpu.num_displays = 3;
    
    /* Features */
    intel_gpu.has_edp = 1;
    intel_gpu.has_hdmi = 1;
    intel_gpu.has_dp = 1;
    intel_gpu.has_quicksync = 1;
    intel_gpu.has_xe_hpg = 0; /* Not Arc */
    
    kprintf("INTEL-i915: Execution Units: %d\n", intel_gpu.execution_units);
    kprintf("INTEL-i915: GPU Frequency: %d MHz\n", intel_gpu.gpu_freq_mhz);
    kprintf("INTEL-i915: Memory: %d MB (shared)\n", intel_gpu.memory_size_mb);
    kprintf("INTEL-i915: Displays: %d\n", intel_gpu.num_displays);
    
    if (intel_gpu.has_quicksync) {
        kprintf("INTEL-i915: QuickSync: Enabled (HW video encode/decode)\n");
    }
    
    /* Display configuration */
    intel_gpu.width = 1920;
    intel_gpu.height = 1080;
    intel_gpu.refresh_rate = 60;
    
    /* Force wake GPU */
    intel_write32(INTEL_REG_FORCEWAKE, 0x00010001);
    
    kprintf("INTEL-i915: Initialized successfully\n");
    
    return 0;
}

int intel_i915_set_mode(uint32_t width, uint32_t height, uint32_t refresh) {
    kprintf("INTEL-i915: Setting mode %dx%d@%dHz\n", width, height, refresh);
    
    intel_gpu.width = width;
    intel_gpu.height = height;
    intel_gpu.refresh_rate = refresh;
    
    /* TODO: Program display pipeline (PIPE A/B/C) */
    
    return 0;
}

int intel_i915_enable_display(uint8_t pipe) {
    if (pipe > 2) return -1;
    
    uint32_t reg = INTEL_REG_PIPEA_CONF + (pipe * 0x1000);
    uint32_t val = intel_read32(reg);
    
    val |= (1 << 31); /* Enable pipe */
    intel_write32(reg, val);
    
    kprintf("INTEL-i915: Display pipe %c enabled\n", 'A' + pipe);
    
    return 0;
}

int intel_i915_get_temperature(void) {
    /* TODO: Read thermal sensors */
    return intel_gpu.temperature;
}

uint32_t intel_i915_get_utilization(void) {
    /* TODO: Read render engine busy status */
    return intel_gpu.gpu_busy_percent;
}

void intel_i915_cleanup(void) {
    /* Disable displays */
    for (int i = 0; i < 3; i++) {
        uint32_t reg = INTEL_REG_PIPEA_CONF + (i * 0x1000);
        intel_write32(reg, 0);
    }
    
    kprintf("INTEL-i915: Driver unloaded (Frames: %d)\n",
            intel_gpu.frames_rendered);
}
