#include "zirvfs.h"
#include "kernel/console.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include <string.h>

extern void *kmalloc(size_t size, unsigned int flags);
extern void kfree(void *ptr);

uint32_t zirvfs_detect_block_size(dev_class_t media_class)
{
    switch (media_class) {
    case DEV_CLASS_SSD:
    case DEV_CLASS_NVME:
        return ZIRVFS_BLOCK_MED;
    case DEV_CLASS_HDD:
    case DEV_CLASS_PATA:
        return ZIRVFS_BLOCK_MAX;
    case DEV_CLASS_PENDRIVE:
    case DEV_CLASS_USB:
        return ZIRVFS_BLOCK_MIN;
    default:
        return ZIRVFS_BLOCK_MED;
    }
}

int zirvfs_read_block(zirvfs_t *fs, uint64_t block_no, void *buf)
{
    if (!fs || !fs->dev_ops || !fs->dev_ops->read_sectors)
        return -1;

    uint64_t sector_per_block = fs->block_size / 512;
    uint64_t start_sector = block_no * sector_per_block;

    int ret = fs->dev_ops->read_sectors(fs->desc, start_sector,
                                         (uint32_t)sector_per_block, buf);
    if (ret < 0)
        return ret;

    return 0;
}

int zirvfs_write_block(zirvfs_t *fs, uint64_t block_no, const void *buf)
{
    if (!fs || !fs->dev_ops || !fs->dev_ops->write_sectors)
        return -1;

    uint64_t sector_per_block = fs->block_size / 512;
    uint64_t start_sector = block_no * sector_per_block;

    int ret = fs->dev_ops->write_sectors(fs->desc, start_sector,
                                          (uint32_t)sector_per_block, buf);
    if (ret < 0)
        return ret;

    return 0;
}

static int zirvfs_bitmap_set(zirvfs_t *fs, uint64_t block_no, int value)
{
    uint64_t word_idx = block_no / 64;
    uint64_t bit_idx  = block_no % 64;
    uint64_t bits_per_block = (fs->block_size * 8);
    uint64_t word_in_bitmap = word_idx / bits_per_block;
    uint64_t word_offset = (word_idx % bits_per_block);

    uint8_t *block_buf = (uint8_t *)kmalloc(fs->block_size, 0);
    if (!block_buf) return -1;

    if (zirvfs_read_block(fs, fs->bitmap_block + word_in_bitmap, block_buf) < 0) {
        kfree(block_buf);
        return -1;
    }

    uint64_t *words = (uint64_t *)block_buf;
    if (value)
        words[word_offset] |= (1ULL << bit_idx);
    else
        words[word_offset] &= ~(1ULL << bit_idx);

    int ret = zirvfs_write_block(fs, fs->bitmap_block + word_in_bitmap, block_buf);
    kfree(block_buf);
    return ret;
}

uint64_t zirvfs_alloc_block(zirvfs_t *fs)
{
    uint64_t bits_per_block = fs->block_size * 8;

    for (uint64_t bm = 0; bm < fs->num_bitmap_blocks; bm++) {
        uint8_t *block_buf = (uint8_t *)kmalloc(fs->block_size, 0);
        if (!block_buf) return 0;

        if (zirvfs_read_block(fs, fs->bitmap_block + bm, block_buf) < 0) {
            kfree(block_buf);
            return 0;
        }

        uint64_t *words = (uint64_t *)block_buf;
        for (uint64_t w = 0; w < bits_per_block / 64; w++) {
            if (words[w] != ~0ULL) {
                uint64_t free_bit = __builtin_ctzll(~words[w]);
                uint64_t global_block = bm * bits_per_block + w * 64 + free_bit;

                if (global_block >= fs->total_blocks) {
                    kfree(block_buf);
                    return 0;
                }

                words[w] |= (1ULL << free_bit);
                zirvfs_write_block(fs, fs->bitmap_block + bm, block_buf);
                kfree(block_buf);

                fs->free_count--;
                return global_block;
            }
        }
        kfree(block_buf);
    }
    return 0;
}

void zirvfs_free_block(zirvfs_t *fs, uint64_t block_no)
{
    if (block_no == 0 || block_no >= fs->total_blocks) return;
    zirvfs_bitmap_set(fs, block_no, 0);
    fs->free_count++;
}

int zirvfs_format(zirvfs_t *fs)
{
    if (!fs || !fs->dev_ops) return -1;

    uint64_t capacity_sectors = 0;
    uint64_t sectors_per_block = fs->block_size / 512;

    if (fs->device) {
        device_desc_t *d = &fs->device->desc;
        capacity_sectors = 0;
        switch (d->bus_class) {
        case DEV_CLASS_NVME:
        case DEV_CLASS_SATA:
            capacity_sectors = 64ULL * 1024 * 1024 / 512;
            break;
        default:
            capacity_sectors = 16ULL * 1024 * 1024 / 512;
            break;
        }
    }

    if (capacity_sectors < 8192) capacity_sectors = 8192;

    fs->total_blocks = capacity_sectors / sectors_per_block;
    if (fs->total_blocks > 0xFFFFFFFFULL)
        fs->total_blocks = 0xFFFFFFFFULL;

    uint64_t bits_needed = fs->total_blocks;
    uint64_t bits_per_block = fs->block_size * 8;
    fs->num_bitmap_blocks = (bits_needed + bits_per_block - 1) / bits_per_block;
    fs->bitmap_block = 1;
    fs->free_count = fs->total_blocks - fs->num_bitmap_blocks - 1;

    klog(LOG_INFO, "ZFS", "Format: %llu blocks, %llu bitmap blocks, block_size=%u",
         (unsigned long long)fs->total_blocks,
         (unsigned long long)fs->num_bitmap_blocks, fs->block_size);

    uint8_t *zero_buf = (uint8_t *)kmalloc(fs->block_size, 0);
    if (!zero_buf) return -1;
    memset(zero_buf, 0, fs->block_size);

    for (uint64_t i = fs->bitmap_block; i < fs->bitmap_block + fs->num_bitmap_blocks; i++)
        zirvfs_write_block(fs, i, zero_buf);

    for (uint64_t i = 0; i < fs->num_bitmap_blocks + 1; i++)
        zirvfs_bitmap_set(fs, i, 1);

    kfree(zero_buf);

    zirvfs_superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    memcpy(sb.magic, ZIRVFS_MAGIC, 8);
    sb.version = ZIRVFS_VERSION;
    sb.block_size = fs->block_size;
    sb.total_blocks = fs->total_blocks;
    sb.root_tree_blk = 0;
    sb.allocation_bitmap_blk = fs->bitmap_block;
    sb.bitmap_blocks = fs->num_bitmap_blocks;
    sb.free_count = fs->free_count;
    sb.commit_count = 0;
    sb.root_commit_blk = 0;
    sb.flags = 0;

    const char *default_label = "ZirvFS Volume";
    size_t lab_len = strlen(default_label);
    if (lab_len > 63) lab_len = 63;
    memcpy(sb.volume_label, default_label, lab_len);

    if (zirvfs_write_block(fs, ZIRVFS_SB_BLOCK, &sb) < 0) return -1;

    uint64_t root_tree_block;
    zirvfs_hash_t root_hash;
    memset(&root_hash, 0, sizeof(root_hash));
    if (zirvfs_tree_create(fs, &root_tree_block, &root_hash) < 0) return -1;

    sb.root_tree_blk = root_tree_block;
    memcpy(sb.volume_label, default_label, lab_len);
    if (zirvfs_write_block(fs, ZIRVFS_SB_BLOCK, &sb) < 0) return -1;

    fs->root_tree_block = root_tree_block;
    fs->root_commit_block = 0;
    fs->commit_count = 0;
    fs->formatted = true;

    klog(LOG_OK, "ZFS", "Format complete, root tree at block %llu",
         (unsigned long long)root_tree_block);
    return 0;
}
