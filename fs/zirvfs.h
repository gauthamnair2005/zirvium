#ifndef ZIRVIUM_FS_ZIRVFS_H
#define ZIRVIUM_FS_ZIRVFS_H

#include "mosix.h"
#include "drivers/zirv/device.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ZIRVFS_MAGIC        "ZIRVFS00"
#define ZIRVFS_VERSION      1
#define ZIRVFS_SB_BLOCK     0
#define ZIRVFS_BLOCK_MIN    4096
#define ZIRVFS_BLOCK_MED    16384
#define ZIRVFS_BLOCK_MAX    65536

#define ZIRVFS_HASH_SIZE    32
#define ZIRVFS_NAME_MAX     255
#define ZIRVFS_MAX_SNAPS    128

typedef enum {
    ZIRVFS_OBJ_NONE   = 0,
    ZIRVFS_OBJ_BLOB   = 1,
    ZIRVFS_OBJ_TREE   = 2,
    ZIRVFS_OBJ_COMMIT = 3,
} zirvfs_obj_type_t;

typedef struct {
    uint8_t  bytes[ZIRVFS_HASH_SIZE];
} __attribute__((packed)) zirvfs_hash_t;

typedef struct {
    uint8_t  magic[8];
    uint32_t version;
    uint32_t block_size;
    uint64_t total_blocks;
    uint64_t root_tree_blk;
    uint64_t allocation_bitmap_blk;
    uint64_t bitmap_blocks;
    uint64_t free_count;
    uint64_t commit_count;
    uint64_t root_commit_blk;
    uint64_t flags;
    uint8_t  volume_label[64];
    uint8_t  padding[3968];
} __attribute__((packed)) zirvfs_superblock_t;

typedef struct {
    uint32_t type;
    uint32_t data_size;
    zirvfs_hash_t hash;
} __attribute__((packed)) zirvfs_obj_header_t;

typedef struct {
    uint32_t mode;
    char     name[ZIRVFS_NAME_MAX + 1];
    zirvfs_hash_t hash;
} __attribute__((packed)) zirvfs_tree_entry_t;

typedef struct {
    uint64_t timestamp;
    zirvfs_hash_t tree_hash;
    zirvfs_hash_t parent_hash;
    char     message[256];
    char     author[64];
    uint64_t snaphsot_id;
} __attribute__((packed)) zirvfs_commit_t;

typedef struct {
    uint64_t id;
    uint64_t timestamp;
    char     message[256];
    zirvfs_hash_t commit_hash;
} __attribute__((packed)) zirvfs_snapshot_info_t;

typedef struct zirvfs {
    registered_device_t   *device;
    const dev_ops_t       *dev_ops;
    device_desc_t         *desc;

    uint32_t               block_size;
    uint32_t               block_shift;
    uint64_t               total_blocks;
    uint64_t               num_bitmap_blocks;
    uint64_t               bitmap_block;
    uint64_t               free_count;

    uint64_t               root_tree_block;
    uint64_t               root_commit_block;
    uint64_t               commit_count;

    vnode_t               *root_vnode;
    vfs_t                  vfs;

    bool                   mounted;
    bool                   formatted;

    struct zirvfs         *next;
} zirvfs_t;

uint32_t zirvfs_detect_block_size(dev_class_t media_class);

int  zirvfs_read_block(zirvfs_t *fs, uint64_t block_no, void *buf);
int  zirvfs_write_block(zirvfs_t *fs, uint64_t block_no, const void *buf);
uint64_t zirvfs_alloc_block(zirvfs_t *fs);
void zirvfs_free_block(zirvfs_t *fs, uint64_t block_no);

void sha256(const uint8_t *data, size_t len, uint8_t out[32]);
void sha256_init(uint32_t state[8]);
void sha256_process(uint32_t state[8], const uint8_t *data, size_t len);
void sha256_final(uint32_t state[8], uint8_t out[32], uint64_t total_bits);
void zirvfs_hash_data(const uint8_t *data, size_t len, zirvfs_hash_t *out);
int  zirvfs_hash_compare(const zirvfs_hash_t *a, const zirvfs_hash_t *b);
void zirvfs_hash_copy(zirvfs_hash_t *dst, const zirvfs_hash_t *src);

int  zirvfs_obj_write(zirvfs_t *fs, uint8_t type, const uint8_t *data, size_t data_size, zirvfs_hash_t *hash, uint64_t *block_out);
int  zirvfs_obj_read(zirvfs_t *fs, const zirvfs_hash_t *hash, uint8_t *type_out, uint8_t *buf, size_t buf_size, size_t *size_out);
int  zirvfs_obj_read_by_block(zirvfs_t *fs, uint64_t block_no, uint8_t *type_out, uint8_t *buf, size_t buf_size, size_t *size_out);

int  zirvfs_tree_create(zirvfs_t *fs, uint64_t *block_out, zirvfs_hash_t *hash);
int  zirvfs_tree_lookup(zirvfs_t *fs, uint64_t tree_block, const char *name, zirvfs_tree_entry_t *entry);
int  zirvfs_tree_insert(zirvfs_t *fs, uint64_t *tree_block, zirvfs_hash_t *tree_hash, const char *name, uint32_t mode, const zirvfs_hash_t *item_hash);
int  zirvfs_tree_remove(zirvfs_t *fs, uint64_t *tree_block, zirvfs_hash_t *tree_hash, const char *name);
int  zirvfs_tree_readdir(zirvfs_t *fs, uint64_t tree_block, uint32_t idx, char *name_out, size_t namelen);

int  zirvfs_commit_create(zirvfs_t *fs, const zirvfs_hash_t *tree_hash, const zirvfs_hash_t *parent_hash, const char *message, uint64_t *commit_block, zirvfs_hash_t *commit_hash);
int  zirvfs_commit_read(zirvfs_t *fs, uint64_t commit_block, zirvfs_commit_t *commit);
int  zirvfs_snapshot_create(zirvfs_t *fs, const char *message, uint64_t *snap_id);
int  zirvfs_snapshot_list(zirvfs_t *fs, zirvfs_snapshot_info_t *snaps, uint32_t *count);
int  zirvfs_snapshot_restore(zirvfs_t *fs, uint64_t snap_id);

int  zirvfs_format(zirvfs_t *fs);
int  zirvfs_mount(zirvfs_t *fs);

void zirvfs_init(void);
zirvfs_t *zirvfs_get_instance(uint32_t index);

#endif
