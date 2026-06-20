#include "zirvfs.h"
#include "kernel/console.h"
#include "kernel/time/time.h"
#include <string.h>

extern void *kmalloc(size_t size, unsigned int flags);
extern void kfree(void *ptr);

int zirvfs_commit_create(zirvfs_t *fs, const zirvfs_hash_t *tree_hash,
                          const zirvfs_hash_t *parent_hash,
                          const char *message,
                          uint64_t *commit_block, zirvfs_hash_t *commit_hash)
{
    zirvfs_commit_t commit;
    memset(&commit, 0, sizeof(commit));

    commit.timestamp = time_uptime_seconds();
    zirvfs_hash_copy(&commit.tree_hash, tree_hash);
    if (parent_hash)
        zirvfs_hash_copy(&commit.parent_hash, parent_hash);
    commit.snaphsot_id = fs->commit_count + 1;

    const char *default_msg = message ? message : "ZirvFS snapshot";
    size_t mlen = strlen(default_msg);
    if (mlen > 255) mlen = 255;
    memcpy(commit.message, default_msg, mlen);

    const char *author = "Zirvium Kernel";
    size_t alen = strlen(author);
    if (alen > 63) alen = 63;
    memcpy(commit.author, author, alen);

    uint64_t block;
    zirvfs_hash_t hash;
    int ret = zirvfs_obj_write(fs, ZIRVFS_OBJ_COMMIT, (const uint8_t *)&commit,
                                sizeof(commit), &hash, &block);
    if (ret < 0) return ret;

    if (commit_block) *commit_block = block;
    if (commit_hash) zirvfs_hash_copy(commit_hash, &hash);

    fs->commit_count++;
    fs->root_commit_block = block;

    zirvfs_superblock_t sb;
    if (zirvfs_read_block(fs, ZIRVFS_SB_BLOCK, &sb) == 0) {
        sb.commit_count = fs->commit_count;
        sb.root_commit_blk = block;
        zirvfs_write_block(fs, ZIRVFS_SB_BLOCK, &sb);
    }

    return 0;
}

int zirvfs_commit_read(zirvfs_t *fs, uint64_t commit_block,
                        zirvfs_commit_t *commit)
{
    if (!commit_block || !commit) return -1;

    uint8_t *buf = (uint8_t *)kmalloc(fs->block_size, 0);
    if (!buf) return -1;

    uint8_t obj_type;
    size_t obj_size;
    if (zirvfs_obj_read_by_block(fs, commit_block, &obj_type, buf, fs->block_size, &obj_size) < 0) {
        kfree(buf);
        return -1;
    }
    if (obj_type != ZIRVFS_OBJ_COMMIT) { kfree(buf); return -1; }

    memcpy(commit, buf + sizeof(zirvfs_obj_header_t), sizeof(zirvfs_commit_t));
    kfree(buf);
    return 0;
}

int zirvfs_snapshot_create(zirvfs_t *fs, const char *message, uint64_t *snap_id)
{
    if (!fs || !fs->formatted) return -1;

    zirvfs_hash_t root_hash;
    {
        uint8_t *buf = (uint8_t *)kmalloc(fs->block_size, 0);
        if (!buf) return -1;
        if (zirvfs_read_block(fs, fs->root_tree_block, buf) < 0) {
            kfree(buf);
            return -1;
        }
        zirvfs_obj_header_t *hdr = (zirvfs_obj_header_t *)buf;
        zirvfs_hash_copy(&root_hash, &hdr->hash);
        kfree(buf);
    }

    zirvfs_hash_t parent_hash;
    memset(&parent_hash, 0, sizeof(parent_hash));

    if (fs->root_commit_block) {
        zirvfs_commit_t parent_commit;
        if (zirvfs_commit_read(fs, fs->root_commit_block, &parent_commit) == 0)
            zirvfs_hash_copy(&parent_hash, &parent_commit.tree_hash);
    }

    uint64_t commit_block;
    zirvfs_hash_t commit_hash;
    if (zirvfs_commit_create(fs, &root_hash, &parent_hash, message,
                              &commit_block, &commit_hash) < 0)
        return -1;

    if (snap_id) *snap_id = fs->commit_count;

    klog(LOG_OK, "ZFS", "Snapshot #%llu created (commit block %llu)",
         (unsigned long long)fs->commit_count, (unsigned long long)commit_block);

    return 0;
}

int zirvfs_snapshot_list(zirvfs_t *fs, zirvfs_snapshot_info_t *snaps,
                          uint32_t *count)
{
    if (!fs || !snaps || !count) return -1;

    uint32_t max_snaps = *count;
    uint32_t written = 0;

    uint64_t current = fs->root_commit_block;
    while (current && written < max_snaps) {
        zirvfs_commit_t commit;
        if (zirvfs_commit_read(fs, current, &commit) < 0) break;

        zirvfs_snapshot_info_t *info = &snaps[written];
        info->id = commit.snaphsot_id;
        info->timestamp = commit.timestamp;
        zirvfs_hash_copy(&info->commit_hash, &commit.tree_hash);
        size_t mlen = strlen(commit.message);
        if (mlen > 255) mlen = 255;
        memcpy(info->message, commit.message, mlen);
        info->message[mlen] = '\0';

        written++;

        if (commit.parent_hash.bytes[0] == 0 &&
            commit.parent_hash.bytes[1] == 0)
            break;

        uint64_t parent_block = 0;
        {
            uint64_t hash64 = 0;
            for (int i = 0; i < 8; i++)
                hash64 = (hash64 << 8) | commit.parent_hash.bytes[i];
            parent_block = (hash64 % (fs->total_blocks - fs->num_bitmap_blocks - 1))
                           + fs->num_bitmap_blocks + 1;
        }
        current = parent_block;
    }

    *count = written;
    return 0;
}

int zirvfs_snapshot_restore(zirvfs_t *fs, uint64_t snap_id)
{
    if (!fs || !fs->formatted) return -1;

    uint64_t current = fs->root_commit_block;
    while (current) {
        zirvfs_commit_t commit;
        if (zirvfs_commit_read(fs, current, &commit) < 0) return -1;

        if (commit.snaphsot_id == snap_id) {
            uint64_t tree_block = 0;
            {
                uint64_t hash64 = 0;
                for (int i = 0; i < 8; i++)
                    hash64 = (hash64 << 8) | commit.tree_hash.bytes[i];
                tree_block = (hash64 % (fs->total_blocks - fs->num_bitmap_blocks - 1))
                             + fs->num_bitmap_blocks + 1;
            }

            fs->root_tree_block = tree_block;

            zirvfs_superblock_t sb;
            if (zirvfs_read_block(fs, ZIRVFS_SB_BLOCK, &sb) == 0) {
                sb.root_tree_blk = tree_block;
                zirvfs_write_block(fs, ZIRVFS_SB_BLOCK, &sb);
            }

            klog(LOG_OK, "ZFS", "Restored snapshot #%llu (tree block %llu)",
                 (unsigned long long)snap_id, (unsigned long long)tree_block);
            return 0;
        }

        if (commit.parent_hash.bytes[0] == 0 &&
            commit.parent_hash.bytes[1] == 0)
            break;

        uint64_t parent_block = 0;
        {
            uint64_t hash64 = 0;
            for (int i = 0; i < 8; i++)
                hash64 = (hash64 << 8) | commit.parent_hash.bytes[i];
            parent_block = (hash64 % (fs->total_blocks - fs->num_bitmap_blocks - 1))
                           + fs->num_bitmap_blocks + 1;
        }
        current = parent_block;
    }

    return -1;
}
