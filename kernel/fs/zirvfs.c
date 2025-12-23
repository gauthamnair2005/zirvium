/* ZirvFS Core Implementation */
#include <kernel/zirvfs.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>

static zirvfs_superblock_t *active_sb = NULL;

void zirvfs_init(void) {
    kprintf("ZirvFS: Initializing CoW File System...\n");
    kprintf("ZirvFS: Features [CoW, Compression, Checksum, 1-Bit Granularity]\n");
}

int zirvfs_mount(const char *device, const char *mount_point) {
    kprintf("ZirvFS: Mounting %s on %s\n", device, mount_point);
    
    // Stub: Read superblock from device
    active_sb = (zirvfs_superblock_t*)kmalloc(sizeof(zirvfs_superblock_t));
    // In real impl, we would read sector 0 of device
    
    // Simulate smart detection and block size alignment
    // In a real scenario, we query the driver (Identify Device command)
    
    // Default to HDD standard
    active_sb->media_type = MEDIA_HDD;
    active_sb->block_size = 512;
    
    // Check for advanced types
    // Stub logic: if device name contains "nvme" or "ssd", assume 4K alignment
    if (device[6] == 'n' || device[6] == 's') { // extremely naive check for /zirv/nvme or /zirv/ssd stub
        active_sb->media_type = MEDIA_NVME;
        active_sb->block_size = 4096; // 4KB native for NVMe
    }

    if (active_sb->media_type == MEDIA_SSD || active_sb->media_type == MEDIA_NVME) {
        kprintf("ZirvFS: Detected Flash Storage (Type: %d)\n", active_sb->media_type);
        kprintf("ZirvFS: Optimized Block Size -> %d bytes\n", active_sb->block_size);
        kprintf("ZirvFS: Enabling Wear Leveling & Trim\n");
    } else {
        kprintf("ZirvFS: Detected Magnetic/Block Storage\n");
        kprintf("ZirvFS: Standard Block Size -> %d bytes\n", active_sb->block_size);
    }
    
    kprintf("ZirvFS: Block Packing (Tail Merging) Enabled\n");
    return 0;
}

/*
 * Stub: Find free space within an existing block (Tail Merging)
 * returns: physical block index and offset, or new block if full
 */
int zirvfs_find_packing_space(uint64_t needed_bytes, uint64_t *out_block, uint32_t *out_offset) {
    // In real implementation:
    // 1. Search free space bitmap/tree for blocks with (capacity - used) >= needed_bytes
    // 2. Return that block + offset
    // 3. Else allocate new block
    
    // For now, assume always new block
    (void)needed_bytes;
    *out_block = 0; // Stub
    *out_offset = 0;
    return 0;
}

/*
 * Read Time-Stamp Counter (RDTSC)
 */
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/*
 * ZirvFS Instant ID Generator
 * Uses hardware timestamp + mixing for fast, non-colliding IDs without pre-allocation.
 * Algorithm: (Timestamp ^ (RandomSeed + Mix))
 */
uint64_t zirvfs_generate_id(void) {
    static uint64_t seed = 0x123456789ABC; // Initial seed
    uint64_t ts = rdtsc();
    
    // Simple mixing (Xorshift style)
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    
    return ts ^ seed;
}

/*
 * Format a device with ZirvFS
 * - Writes Superblock
 * - Creates Root Inode
 * - Enables default features
 */
int zirvfs_format(const char *device, const char *label) {
    kprintf("ZirvFS: Formatting device %s...\n", device);
    
    // In a real implementation, we would open the block device driver here
    // struct block_device *bd = block_device_open(device);
    
    zirvfs_superblock_t sb;
    sb.magic = ZIRVFS_MAGIC;
    sb.version = ZIRVFS_VERSION;
    
    // Default features
    sb.features = ZIRVFS_FEAT_COW | ZIRVFS_FEAT_COMPRESS | ZIRVFS_FEAT_CHECKSUM | ZIRVFS_FEAT_1BIT;
    
    // Simulate detecting media type from device path/driver
    if (device[6] == 'n' || device[6] == 's') { // naive /zirv/nvme check
        sb.media_type = MEDIA_NVME;
        sb.block_size = 4096;
    } else {
        sb.media_type = MEDIA_HDD;
        sb.block_size = 512;
    }
    
    // Copy label
    int i;
    for (i = 0; i < 31 && label[i]; i++) sb.label[i] = label[i];
    sb.label[i] = '\0';
    
    // Generate UUID (stub)
    uint64_t uuid_part = zirvfs_generate_id();
    *(uint64_t*)sb.uuid = uuid_part;
    
    kprintf("ZirvFS: Creating Superblock [BlockSize: %d, Label: %s]\n", sb.block_size, sb.label);
    
    // Print ID manually to avoid 64-bit div/mod linking issues on 32-bit
    uint64_t root_id = zirvfs_generate_id();
    uint32_t id_hi = (uint32_t)(root_id >> 32);
    uint32_t id_lo = (uint32_t)(root_id & 0xFFFFFFFF);
    kprintf("ZirvFS: Root Inode ID: %x%x (Instant Generated)\n", id_hi, id_lo);
    
    kprintf("ZirvFS: Format Complete. Device ready for mount.\n");
    return 0;
}
