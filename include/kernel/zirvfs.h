#ifndef KERNEL_ZIRVFS_H
#define KERNEL_ZIRVFS_H

#include <kernel/types.h>

/* ZirvFS Constants */
#define ZIRVFS_MAGIC 0x21E7F5 /* ZI(21)E7F5 */
#define ZIRVFS_VERSION 1

/* Feature Flags */
#define ZIRVFS_FEAT_COW         (1 << 0)
#define ZIRVFS_FEAT_COMPRESS    (1 << 1)
#define ZIRVFS_FEAT_CHECKSUM    (1 << 2)
#define ZIRVFS_FEAT_1BIT        (1 << 3)

/* Compression Types */
#define ZIRVFS_COMP_NONE    0
#define ZIRVFS_COMP_ZLIB    1
#define ZIRVFS_COMP_LZ4     2
#define ZIRVFS_COMP_ZSTD    3

/* Storage Media Types (Smart Drive Determiner) */
typedef enum {
    MEDIA_UNKNOWN = 0,
    MEDIA_HDD = 1,      /* Mechanical Hard Drive */
    MEDIA_SSD = 2,      /* Solid State Drive */
    MEDIA_NVME = 3,     /* NVMe SSD */
    MEDIA_USB = 4,      /* USB Flash Drive */
    MEDIA_RAM = 5,      /* RAM Disk */
    MEDIA_OPTICAL = 6   /* CD/DVD/Bluray */
} media_type_t;

/* 
 * ZirvFS Superblock 
 * Represents the root of a ZirvFS partition/subvolume
 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;        /* Dynamic Block Size (e.g., 512, 4096) */
    uint64_t total_blocks;      /* Total capacity in blocks */
    uint64_t free_blocks;       /* Free space in blocks */
    uint64_t root_inode_ptr;    /* Pointer to root inode */
    uint32_t features;          /* Enabled features */
    media_type_t media_type;    /* Detected media type */
    uint8_t uuid[16];           /* Unique ID */
    char label[32];             /* Volume Label */
} __attribute__((packed)) zirvfs_superblock_t;

/*
 * ZirvFS Inode
 * Represents a file or directory
 */
typedef struct {
    uint64_t inode_id;
    uint64_t size_bytes;        /* Logical size in bytes (precise) */
    uint64_t permissions;
    uint64_t created_at;
    uint64_t modified_at;
    uint32_t compression_algo;  /* Compression algorithm used */
    uint32_t checksum;          /* Integrity checksum */
    uint64_t data_block_ptr;    /* Pointer to first data block */
    uint64_t parent_inode;      /* Backpointer to parent */
    uint32_t ref_count;         /* Reference count for CoW */
} __attribute__((packed)) zirvfs_inode_t;

/*
 * ZirvFS Extent
 * Replaces chunk. Represents a contiguous range of blocks/bytes.
 * Supports Block Packing: Multiple small files can share the same physical block.
 */
typedef struct {
    uint64_t logical_offset;    /* Logical offset in file */
    uint64_t start_block;       /* Physical start block on disk */
    uint32_t block_offset;      /* Byte offset within the physical block */
    uint32_t length_bytes;      /* Exact length in bytes (allows sub-block packing) */
    uint32_t checksum;          /* Checksum of this extent */
} __attribute__((packed)) zirvfs_extent_t;

void zirvfs_init(void);
int zirvfs_mount(const char *device, const char *mount_point);
int zirvfs_format(const char *device, const char *label);

#endif
