/* drivers/zirv/sata.c
 * Zirvium Kernel — SATA / ATA device driver
 *
 * Uses the legacy ATA PIO interface for initial bring-up.
 * Supports detection of HDD and SSD devices on the two primary/secondary
 * ATA channels.  CD-ROM / CD-RW detection is done via the PACKET command
 * check in the IDENTIFY response.
 *
 * Detected devices are registered with the /zirv device registry so that
 * they appear as:
 *   /zirv/sata/hdd0, /zirv/sata/hdd1, …
 *   /zirv/sata/ssd0, /zirv/sata/ssd1, …
 *   /zirv/sata/cdrom0, /zirv/sata/cdrw0, …
 */
#include "device.h"
#include "../../arch/x64/cpu.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* ── ATA register offsets (relative to base I/O port) ────────────────────── */
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01   /* read  */
#define ATA_REG_FEATURES    0x01   /* write */
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA0        0x03
#define ATA_REG_LBA1        0x04
#define ATA_REG_LBA2        0x05
#define ATA_REG_HDDEVSEL    0x06
#define ATA_REG_STATUS      0x07   /* read  */
#define ATA_REG_COMMAND     0x07   /* write */

/* Control register (base + 0x206 for primary, 0x306 for secondary) */
#define ATA_REG_CONTROL     0x00   /* relative to ctrl_base */
#define ATA_REG_ALTSTATUS   0x00   /* read  */

/* ── ATA status bits ──────────────────────────────────────────────────────── */
#define ATA_SR_BUSY  0x80   /* BSY */
#define ATA_SR_DRDY  0x40   /* DRDY */
#define ATA_SR_DRQ   0x08   /* DRQ */
#define ATA_SR_ERR   0x01   /* ERR */

/* ── ATA commands ─────────────────────────────────────────────────────────── */
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_READ_SECTORS    0x20
#define ATA_CMD_WRITE_SECTORS   0x30
#define ATA_CMD_FLUSH           0xE7

/* ── Known ATA channel I/O addresses ─────────────────────────────────────── */
typedef struct {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint16_t bmide_base;   /* bus-master IDE base (not used in PIO mode) */
    uint8_t  no_slave;
} ata_channel_t;

static ata_channel_t ata_channels[2] = {
    { 0x1F0, 0x3F6, 0x000, 0 },   /* primary   */
    { 0x170, 0x376, 0x000, 0 },   /* secondary */
};

/* ── Per-drive state ──────────────────────────────────────────────────────── */
typedef struct {
    bool     exists;
    bool     is_atapi;        /* PACKET (CD-ROM) device */
    uint8_t  channel;         /* 0 = primary, 1 = secondary */
    uint8_t  drive;           /* 0 = master, 1 = slave */
    uint32_t lba28_sectors;   /* from IDENTIFY word 60-61 */
    uint64_t lba48_sectors;   /* from IDENTIFY word 100-103 */
    bool     supports_lba48;
    char     model[41];       /* from IDENTIFY bytes 54-93 */
    char     serial[21];      /* from IDENTIFY bytes 20-39 */
    bool     is_ssd;          /* heuristic: no seek-time in IDENTIFY */
    bool     has_cdrw;        /* disc can be written (ATAPI) */
} ata_drive_t;

static ata_drive_t ata_drives[4];   /* [ch*2 + drive] */

/* ── ATA PIO helpers ──────────────────────────────────────────────────────── */
static void ata_delay400ns(uint8_t ch)
{
    /* Read the alt-status register 4 times ≈ 400 ns */
    for (int i = 0; i < 4; i++)
        inb((uint16_t)(ata_channels[ch].ctrl_base + ATA_REG_ALTSTATUS));
}

static int ata_poll(uint8_t ch, bool check_drq)
{
    ata_delay400ns(ch);
    /* Wait for BSY to clear */
    for (int t = 0; t < 100000; t++) {
        uint8_t st = inb((uint16_t)(ata_channels[ch].io_base + ATA_REG_STATUS));
        if (!(st & ATA_SR_BUSY)) {
            if (check_drq && !(st & ATA_SR_DRQ)) return -1;
            if (st & ATA_SR_ERR) return -2;
            return 0;
        }
    }
    return -3;   /* timeout */
}

/* Copy and swap ATA string bytes (big-endian word pairs) into a C string */
static void ata_copy_string(char *dst, const uint16_t *words,
                             int word_start, int word_count)
{
    for (int w = 0; w < word_count; w++) {
        uint16_t word = words[word_start + w];
        dst[w * 2]     = (char)(word >> 8);
        dst[w * 2 + 1] = (char)(word & 0xFF);
    }
    dst[word_count * 2] = '\0';
    /* Trim trailing spaces */
    for (int i = word_count * 2 - 1; i >= 0 && dst[i] == ' '; i--)
        dst[i] = '\0';
}

/* ── IDENTIFY ──────────────────────────────────────────────────────────────── */
static bool ata_identify(uint8_t ch, uint8_t drive, ata_drive_t *out)
{
    uint16_t base = ata_channels[ch].io_base;

    /* Select drive */
    outb((uint16_t)(base + ATA_REG_HDDEVSEL), (uint8_t)(0xA0 | (drive << 4)));
    ata_delay400ns(ch);

    /* Zero sector count / LBA registers */
    outb((uint16_t)(base + ATA_REG_SECCOUNT), 0);
    outb((uint16_t)(base + ATA_REG_LBA0),     0);
    outb((uint16_t)(base + ATA_REG_LBA1),     0);
    outb((uint16_t)(base + ATA_REG_LBA2),     0);

    /* Send IDENTIFY */
    outb((uint16_t)(base + ATA_REG_COMMAND), ATA_CMD_IDENTIFY);
    ata_delay400ns(ch);

    /* If status == 0 no drive is present */
    uint8_t st = inb((uint16_t)(base + ATA_REG_STATUS));
    if (st == 0) return false;

    /* Check for ATAPI */
    bool is_atapi = false;
    uint8_t cl = inb((uint16_t)(base + ATA_REG_LBA1));
    uint8_t ch2 = inb((uint16_t)(base + ATA_REG_LBA2));
    if (cl == 0x14 && ch2 == 0xEB) {
        is_atapi = true;
        outb((uint16_t)(base + ATA_REG_COMMAND), ATA_CMD_IDENTIFY_PACKET);
        ata_delay400ns(ch);
    }

    if (ata_poll(ch, true) != 0) return false;

    /* Read 256 words of IDENTIFY data */
    uint16_t buf[256];
    for (int i = 0; i < 256; i++)
        buf[i] = inw(base);

    out->exists   = true;
    out->is_atapi = is_atapi;
    out->channel  = ch;
    out->drive    = drive;

    ata_copy_string(out->model,  buf, 27, 20);
    ata_copy_string(out->serial, buf, 10, 10);

    /* LBA48 support */
    out->supports_lba48   = (buf[83] & (1 << 10)) != 0;
    out->lba28_sectors    = ((uint32_t)buf[61] << 16) | buf[60];
    if (out->supports_lba48) {
        out->lba48_sectors = (uint64_t)buf[100]
                           | ((uint64_t)buf[101] << 16)
                           | ((uint64_t)buf[102] << 32)
                           | ((uint64_t)buf[103] << 48);
    }

    /* Heuristic: SATA SSDs set the Nominal Media Rotation Rate (word 217)
     * to 0x0001 ("Non-Rotating Media"). */
    out->is_ssd = (!is_atapi && buf[217] == 0x0001);

    /* ATAPI writeable hint: check if GET CONFIGURATION would report CD-RW.
     * For simplicity we mark ATAPI drives as potentially rewriteable if
     * word 0 bit 9 (removable media) is set — real drivers would use
     * the PACKET interface to query capabilities. */
    out->has_cdrw = is_atapi && ((buf[0] & (1 << 7)) != 0);

    return true;
}

/* ── Driver ops ───────────────────────────────────────────────────────────── */
static int sata_dev_init(device_desc_t *dev)
{
    (void)dev;   /* hardware already probed in sata_init() */
    return 0;
}

static int sata_read_sectors(device_desc_t *dev, uint64_t lba,
                              uint32_t count, void *buf)
{
    /* Retrieve the ata_drive_t from driver_data */
    ata_drive_t *d = (ata_drive_t *)dev->driver_data;
    if (!d || !d->exists) return -1;

    uint16_t base = ata_channels[d->channel].io_base;
    uint16_t *pbuf = (uint16_t *)buf;

    for (uint32_t s = 0; s < count; s++, lba++) {
        /* Wait for drive to be ready */
        if (ata_poll(d->channel, false) != 0) return -(int)s;

        if (d->supports_lba48) {
            /* LBA48 mode */
            outb((uint16_t)(base + ATA_REG_HDDEVSEL),
                 (uint8_t)(0x40 | (d->drive << 4)));
            outb((uint16_t)(base + ATA_REG_SECCOUNT), 0);   /* high byte */
            outb((uint16_t)(base + ATA_REG_LBA0), (uint8_t)(lba >> 24));
            outb((uint16_t)(base + ATA_REG_LBA1), (uint8_t)(lba >> 32));
            outb((uint16_t)(base + ATA_REG_LBA2), (uint8_t)(lba >> 40));
            outb((uint16_t)(base + ATA_REG_SECCOUNT), 1);   /* low byte */
            outb((uint16_t)(base + ATA_REG_LBA0), (uint8_t)(lba));
            outb((uint16_t)(base + ATA_REG_LBA1), (uint8_t)(lba >> 8));
            outb((uint16_t)(base + ATA_REG_LBA2), (uint8_t)(lba >> 16));
            outb((uint16_t)(base + ATA_REG_COMMAND), 0x24); /* READ SECTORS EXT */
        } else {
            /* LBA28 mode */
            outb((uint16_t)(base + ATA_REG_HDDEVSEL),
                 (uint8_t)(0xE0 | (d->drive << 4) | ((lba >> 24) & 0x0F)));
            outb((uint16_t)(base + ATA_REG_SECCOUNT), 1);
            outb((uint16_t)(base + ATA_REG_LBA0), (uint8_t)(lba));
            outb((uint16_t)(base + ATA_REG_LBA1), (uint8_t)(lba >> 8));
            outb((uint16_t)(base + ATA_REG_LBA2), (uint8_t)(lba >> 16));
            outb((uint16_t)(base + ATA_REG_COMMAND), ATA_CMD_READ_SECTORS);
        }

        if (ata_poll(d->channel, true) != 0) return -(int)s;

        /* Read 256 words (512 bytes) */
        for (int w = 0; w < 256; w++)
            pbuf[s * 256 + w] = inw(base);
    }
    return (int)count;
}

static int sata_write_sectors(device_desc_t *dev, uint64_t lba,
                               uint32_t count, const void *buf)
{
    ata_drive_t *d = (ata_drive_t *)dev->driver_data;
    if (!d || !d->exists) return -1;

    uint16_t base = ata_channels[d->channel].io_base;
    const uint16_t *pbuf = (const uint16_t *)buf;

    for (uint32_t s = 0; s < count; s++, lba++) {
        if (ata_poll(d->channel, false) != 0) return -(int)s;

        if (d->supports_lba48) {
            outb((uint16_t)(base + ATA_REG_HDDEVSEL),
                 (uint8_t)(0x40 | (d->drive << 4)));
            outb((uint16_t)(base + ATA_REG_SECCOUNT), 0);
            outb((uint16_t)(base + ATA_REG_LBA0), (uint8_t)(lba >> 24));
            outb((uint16_t)(base + ATA_REG_LBA1), (uint8_t)(lba >> 32));
            outb((uint16_t)(base + ATA_REG_LBA2), (uint8_t)(lba >> 40));
            outb((uint16_t)(base + ATA_REG_SECCOUNT), 1);
            outb((uint16_t)(base + ATA_REG_LBA0), (uint8_t)(lba));
            outb((uint16_t)(base + ATA_REG_LBA1), (uint8_t)(lba >> 8));
            outb((uint16_t)(base + ATA_REG_LBA2), (uint8_t)(lba >> 16));
            outb((uint16_t)(base + ATA_REG_COMMAND), 0x34); /* WRITE SECTORS EXT */
        } else {
            outb((uint16_t)(base + ATA_REG_HDDEVSEL),
                 (uint8_t)(0xE0 | (d->drive << 4) | ((lba >> 24) & 0x0F)));
            outb((uint16_t)(base + ATA_REG_SECCOUNT), 1);
            outb((uint16_t)(base + ATA_REG_LBA0), (uint8_t)(lba));
            outb((uint16_t)(base + ATA_REG_LBA1), (uint8_t)(lba >> 8));
            outb((uint16_t)(base + ATA_REG_LBA2), (uint8_t)(lba >> 16));
            outb((uint16_t)(base + ATA_REG_COMMAND), ATA_CMD_WRITE_SECTORS);
        }

        if (ata_poll(d->channel, true) != 0) return -(int)s;

        for (int w = 0; w < 256; w++)
            outw(base, pbuf[s * 256 + w]);

        /* Flush cache */
        outb((uint16_t)(base + ATA_REG_COMMAND), ATA_CMD_FLUSH);
        ata_poll(d->channel, false);
    }
    return (int)count;
}

static const dev_ops_t sata_ops = {
    .init          = sata_dev_init,
    .read_sectors  = sata_read_sectors,
    .write_sectors = sata_write_sectors,
    .ioctl         = NULL,
    .shutdown      = NULL,
};

/* ── SATA/PATA init: probe all four drives ───────────────────────────────── */
void sata_init(void)
{
    for (uint8_t ch = 0; ch < 2; ch++) {
        for (uint8_t dr = 0; dr < 2; dr++) {
            int idx = ch * 2 + dr;
            ata_drives[idx].exists = false;

            if (!ata_identify(ch, dr, &ata_drives[idx]))
                continue;

            ata_drive_t *d = &ata_drives[idx];
            dev_class_t bus   = (ch == 0) ? DEV_CLASS_SATA : DEV_CLASS_SATA;
            /* For PATA (legacy controller at same addresses without AHCI),
             * the caller would substitute DEV_CLASS_PATA. In this simplified
             * driver we always report SATA for the primary/secondary ATA bus. */
            dev_class_t media;
            if (d->is_atapi) {
                media = d->has_cdrw ? DEV_CLASS_CDRW : DEV_CLASS_CDROM;
            } else {
                media = d->is_ssd ? DEV_CLASS_SSD : DEV_CLASS_HDD;
            }

            registered_device_t *reg =
                zirv_register_device(bus, media, d->model, &sata_ops);
            if (reg)
                reg->desc.driver_data = d;
        }
    }
}
