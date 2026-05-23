#include "ac97.h"
#include "kernel/audio/audio.h"
#include "drivers/pci/pci.h"
#include "drivers/serial/serial.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/irq/irq.h"
#include "arch/x64/cpu.h"
#include <stdint.h>
#include <string.h>

/* ── PCI identification ──────────────────────────────────────────────────── */
#define AC97_VENDOR_INTEL  0x8086
#define PCI_CLASS_MM        0x04
#define PCI_SUBCLASS_AUDIO  0x00

static const uint16_t ac97_device_ids[] = {
    0x2415, 0x2425, 0x2445, 0x2455, 0x2485, 0x24C5,
    0x266E, 0x27DE, 0x2698, 0x284B, 0x293E, 0x294E,
    0x2A6E, 0x2B6E, 0x2D6E, 0x3B56, 0x3B57, 0x1C20,
    0x1E20, 0x8CA0, 0x8D20, 0x9CA0, 0x9D20, 0xA170,
    0xA1A0, 0xA270,
    0xFFFF  /* terminator */
};

/* ── NAM (Mixer) I/O register offsets ─────────────────────────────────────── */
#define AC97_NAM_RESET      0x00
#define AC97_NAM_MASTER_VOL 0x02
#define AC97_NAM_PCM_VOL    0x18
#define AC97_NAM_EXTENDED   0x28
#define AC97_NAM_VENDOR_ID1 0x7C
#define AC97_NAM_VENDOR_ID2 0x7E

/* ── NABM (Bus Master) I/O register offsets ───────────────────────────────── */
#define AC97_PO_BDBAR   0x10   /* PCM Out Buffer Descriptor Base Address */
#define AC97_PO_CIV     0x14   /* Current Index Value (byte)            */
#define AC97_PO_LVI     0x15   /* Last Valid Index (byte)               */
#define AC97_PO_SR      0x16   /* Status Register (byte)                */
#define AC97_PO_CR      0x17   /* Control Register (byte)               */

/* Status/Control bits */
#define AC97_SR_DCH     (1 << 0)   /* DMA Controller Halted    */
#define AC97_SR_CELV    (1 << 1)   /* Current End of List      */
#define AC97_SR_LVBCI   (1 << 2)   /* Last Valid Buffer Complete */
#define AC97_SR_BCIS    (1 << 3)   /* Buffer Completion Interrupt */
#define AC97_SR_FIFOE   (1 << 4)   /* FIFO Error */
#define AC97_CR_RUN     (1 << 0)   /* Start DMA               */
#define AC97_CR_RESET   (1 << 1)   /* Reset DMA               */
#define AC97_CR_IOCE    (1 << 2)   /* Interrupt On Completion */
#define AC97_CR_FEIE    (1 << 3)   /* FIFO Error Interrupt    */

/* ── Buffer descriptor entry (32-bit) ─────────────────────────────────────── */
/* Bits 31:2 = phys addr, bit 1 = IOC, bit 0 = BUP */
#define AC97_BD_ENTRY(phys, ioc) \
    (((uint32_t)(phys) & ~3u) | ((ioc) ? 2u : 0u))

#define AC97_NUM_DESCS    4
#define AC97_BUF_FRAMES   256
#define AC97_BUF_BYTES    (AC97_BUF_FRAMES * 4)  /* 16-bit stereo */

/* ── Per-driver state ─────────────────────────────────────────────────────── */
static struct {
    pci_dev_t *pdev;
    uint16_t   iobase_nam;    /* NAM I/O base */
    uint16_t   iobase_nabm;   /* NABM I/O base */
    int        irq;

    uint32_t   bdl_phys;       /* physical address of BDL array */
    uint32_t  *bdl;            /* virtual address */
    void      *pcm_bufs[AC97_NUM_DESCS];
    uint32_t   pcm_bufs_phys[AC97_NUM_DESCS];
    int        cur_write;      /* next buffer to fill */
    uint8_t    cur_play;       /* current buffer being played */
    uint8_t    completed;      /* mask of completed buffers */
    bool       running;
    audio_driver_t driver;
} g_ac97;

/* ── I/O helpers ──────────────────────────────────────────────────────────── */
static inline uint8_t ac97_nam_read8(uint16_t reg)
{
    return inb(g_ac97.iobase_nam + reg);
}
static inline void ac97_nam_write8(uint16_t reg, uint8_t val)
{
    outb(g_ac97.iobase_nam + reg, val);
}
static inline uint16_t ac97_nam_read16(uint16_t reg)
{
    return inw(g_ac97.iobase_nam + reg);
}
static inline void ac97_nam_write16(uint16_t reg, uint16_t val)
{
    outw(g_ac97.iobase_nam + reg, val);
}
static inline uint8_t ac97_nabm_read8(uint16_t reg)
{
    return inb(g_ac97.iobase_nabm + reg);
}
static inline void ac97_nabm_write8(uint16_t reg, uint8_t val)
{
    outb(g_ac97.iobase_nabm + reg, val);
}
static inline uint32_t ac97_nabm_read32(uint16_t reg)
{
    return inl(g_ac97.iobase_nabm + reg);
}
static inline void ac97_nabm_write32(uint16_t reg, uint32_t val)
{
    outl(g_ac97.iobase_nabm + reg, val);
}

/* ── IRQ handler ──────────────────────────────────────────────────────────── */
static int ac97_irq_handler(int irq, void *data)
{
    (void)irq; (void)data;
    uint8_t sr = ac97_nabm_read8(AC97_PO_SR);
    if (!(sr & (AC97_SR_BCIS | AC97_SR_LVBCI | AC97_SR_FIFOE)))
        return IRQ_NONE;

    /* Clear status bits (write 1 to clear) */
    ac97_nabm_write8(AC97_PO_SR, sr);

    /* Update which buffer is being played */
    g_ac97.cur_play = ac97_nabm_read8(AC97_PO_CIV);

    /* Mark the buffer that just completed as free.
     * CIV is the buffer being played NOW; the one before it just finished. */
    uint8_t completed = (uint8_t)((g_ac97.cur_play - 1 + AC97_NUM_DESCS) % AC97_NUM_DESCS);
    g_ac97.completed |= (uint8_t)(1u << completed);

    return IRQ_HANDLED;
}

/* ── Audio driver interface ───────────────────────────────────────────────── */
static uint32_t ac97_write_pcm(const void *buf, uint32_t frames)
{
    if (!buf || frames == 0) return 0;
    if (!g_ac97.running) return 0;

    uint32_t written = 0;
    const uint8_t *src = (const uint8_t *)buf;

    while (written < frames) {
        int idx = g_ac97.cur_write;

        /* Check if this buffer has been consumed by hardware */
        if (!(g_ac97.completed & (1u << idx)))
            break;  /* no free slot right now — return what we've written so far */

        g_ac97.completed &= (uint8_t)~(1u << idx);  /* claim this slot */

        uint32_t copy = frames - written;
        if (copy > AC97_BUF_FRAMES) copy = AC97_BUF_FRAMES;

        memcpy(g_ac97.pcm_bufs[idx], src, copy * 4);
        if (copy < AC97_BUF_FRAMES)
            memset((uint8_t*)g_ac97.pcm_bufs[idx] + copy * 4, 0,
                   (AC97_BUF_FRAMES - copy) * 4);

        src += copy * 4;
        written += copy;

        /* Update LVI so the hardware knows there's data in this buffer */
        ac97_nabm_write8(AC97_PO_LVI, (uint8_t)idx);

        g_ac97.cur_write = (idx + 1) % AC97_NUM_DESCS;
    }

    return written;
}

static void ac97_set_volume(uint8_t vol)
{
    (void)vol;
    /* Map 0-255 to AC97 volume (0=max, 63=min) */
    uint16_t v = (uint16_t)((255 - (uint32_t)vol) * 63 / 255);
    uint16_t reg = (uint16_t)((v << 8) | v);
    ac97_nam_write16(AC97_NAM_MASTER_VOL, reg);
}

static void ac97_start(void)
{
    if (g_ac97.running) return;

    /* Reset the PCM out engine */
    ac97_nabm_write8(AC97_PO_CR, AC97_CR_RESET);
    uint16_t timeout = 0;
    while (ac97_nabm_read8(AC97_PO_CR) & AC97_CR_RESET) {
        if (++timeout > 10000) break;
    }

    /* Fill all buffers with silence */
    for (int i = 0; i < AC97_NUM_DESCS; i++)
        memset(g_ac97.pcm_bufs[i], 0, AC97_BUF_BYTES);

    /* Set up BDL entries */
    for (int i = 0; i < AC97_NUM_DESCS; i++)
        g_ac97.bdl[i] = AC97_BD_ENTRY(g_ac97.pcm_bufs_phys[i], 1);

    /* Write BDL base address */
    ac97_nabm_write32(AC97_PO_BDBAR, g_ac97.bdl_phys);

    /* Set last valid index */
    ac97_nabm_write8(AC97_PO_LVI, (uint8_t)(AC97_NUM_DESCS - 1));

    /* Clear status */
    ac97_nabm_write8(AC97_PO_SR, 0xFF);

    /* Start: RUN | IOCE */
    ac97_nabm_write8(AC97_PO_CR, AC97_CR_RUN | AC97_CR_IOCE);

    g_ac97.cur_write = 0;
    g_ac97.cur_play = 0;
    g_ac97.completed = 0;  /* no buffers completed yet; first BCIS will set a bit */
    g_ac97.running = true;
    serial_puts(SERIAL_COM1, "[ac97] Playback started\n");
}

static void ac97_stop(void)
{
    if (!g_ac97.running) return;
    ac97_nabm_write8(AC97_PO_CR, 0);
    g_ac97.running = false;
    serial_puts(SERIAL_COM1, "[ac97] Playback stopped\n");
}

/* ── Initialisation ───────────────────────────────────────────────────────── */
void ac97_init(void)
{
    serial_puts(SERIAL_COM1, "[ac97] Scanning PCI for AC97 controller\n");

    pci_dev_t *pdev = NULL;
    for (int i = 0; ac97_device_ids[i] != 0xFFFF; i++) {
        pdev = pci_find_device(AC97_VENDOR_INTEL, ac97_device_ids[i]);
        if (pdev) break;
    }
    if (!pdev)
        pdev = pci_find_device_by_class(PCI_CLASS_MM, PCI_SUBCLASS_AUDIO);

    if (!pdev) {
        serial_puts(SERIAL_COM1, "[ac97] No AC97 controller found\n");
        return;
    }

    serial_puts(SERIAL_COM1, "[ac97] AC97 controller found\n");
    pci_enable_device(pdev);

    g_ac97.pdev = pdev;
    g_ac97.irq  = pdev->irq_line;

    /* Map BAR0 (NAM) and BAR1 (NABM) - both I/O ports for AC97 */
    pci_bar_t *bar0 = &pdev->bars[0];
    pci_bar_t *bar1 = &pdev->bars[1];
    if (!bar0->is_io || !bar1->is_io) {
        serial_puts(SERIAL_COM1, "[ac97] BARs are not I/O (unexpected)\n");
        return;
    }
    g_ac97.iobase_nam  = (uint16_t)(bar0->phys_addr & 0xFFFF);
    g_ac97.iobase_nabm = (uint16_t)(bar1->phys_addr & 0xFFFF);

    /* Reset AC97 codec via NAM reset */
    ac97_nam_write16(AC97_NAM_RESET, 1);
    uint16_t timeout = 0;
    while (ac97_nam_read16(AC97_NAM_RESET) & 1) {
        if (++timeout > 10000) break;
    }

    /* If reset register reads back 0, check vendor ID to confirm presence */
    uint16_t vid1 = ac97_nam_read16(AC97_NAM_VENDOR_ID1);
    if (vid1 == 0xFFFF || vid1 == 0) {
        serial_puts(SERIAL_COM1, "[ac97] No AC97 codec detected\n");
        return;
    }

    /* Set master volume to max (0 = loudest) */
    ac97_nam_write16(AC97_NAM_MASTER_VOL, 0x0000);
    /* Set PCM volume to max */
    ac97_nam_write16(AC97_NAM_PCM_VOL, 0x0000);

    /* Allocate BDL */
    g_ac97.bdl_phys = (uint32_t)pmm_alloc_page();
    if (!g_ac97.bdl_phys) {
        serial_puts(SERIAL_COM1, "[ac97] BDL alloc failed\n");
        return;
    }
    g_ac97.bdl = (uint32_t*)PHYS_TO_VIRT((uint64_t)g_ac97.bdl_phys);

    /* Allocate PCM buffers */
    for (int i = 0; i < AC97_NUM_DESCS; i++) {
        uint64_t pa = pmm_alloc_page();
        if (!pa) {
            serial_puts(SERIAL_COM1, "[ac97] PCM buf alloc failed\n");
            return;
        }
        g_ac97.pcm_bufs_phys[i] = (uint32_t)pa;
        g_ac97.pcm_bufs[i] = PHYS_TO_VIRT(pa);
        memset(g_ac97.pcm_bufs[i], 0, AC97_BUF_BYTES);
    }

    /* Request IRQ */
    if (g_ac97.irq > 0 && g_ac97.irq < 16) {
        request_irq(g_ac97.irq, ac97_irq_handler, IRQF_SHARED,
                    "ac97", NULL);
    }

    /* Register with audio subsystem */
    g_ac97.driver.name     = "Intel AC97";
    g_ac97.driver.write_pcm = ac97_write_pcm;
    g_ac97.driver.set_volume = ac97_set_volume;
    g_ac97.driver.start     = ac97_start;
    g_ac97.driver.stop      = ac97_stop;
    g_ac97.driver.ready     = true;

    audio_register(&g_ac97.driver);

    /* Start playback */
    ac97_start();

    serial_puts(SERIAL_COM1, "[ac97] AC97 ready on I/O 0x");
    serial_puts(SERIAL_COM1, "NAM=");
    { char h[6]; int n = 0;
      uint16_t t = g_ac97.iobase_nam;
      for (int s = 12; s >= 0; s -= 4) { int d = (t >> s) & 0xF;
        h[n++] = (char)(d < 10 ? '0'+d : 'A'+d-10); }
      h[n] = 0; serial_puts(SERIAL_COM1, h); }
    serial_puts(SERIAL_COM1, " NABM=");
    { char h[6]; int n = 0;
      uint16_t t = g_ac97.iobase_nabm;
      for (int s = 12; s >= 0; s -= 4) { int d = (t >> s) & 0xF;
        h[n++] = (char)(d < 10 ? '0'+d : 'A'+d-10); }
      h[n] = 0; serial_puts(SERIAL_COM1, h); }
    serial_puts(SERIAL_COM1, "\n");
}
