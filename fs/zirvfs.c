#include "zirvfs.h"
#include "kernel/console.h"
#include "kernel/mm/pmm.h"
#include "kernel/time/time.h"
#include <string.h>

extern void *kmalloc(size_t size, unsigned int flags);
extern void kfree(void *ptr);

static zirvfs_t *zirvfs_instances = NULL;
static uint32_t zirvfs_instance_count = 0;

zirvfs_t *zirvfs_get_instance(uint32_t index)
{
    zirvfs_t *cur = zirvfs_instances;
    for (uint32_t i = 0; cur && i < index; i++)
        cur = cur->next;
    return cur;
}

int zirvfs_obj_write(zirvfs_t *fs, uint8_t type, const uint8_t *data,
                      size_t data_size, zirvfs_hash_t *hash, uint64_t *block_out)
{
    size_t total_size = sizeof(zirvfs_obj_header_t) + data_size;
    uint32_t blocks_needed = (uint32_t)((total_size + fs->block_size - 1) / fs->block_size);

    uint8_t *buf = (uint8_t *)kmalloc(blocks_needed * fs->block_size, 0);
    if (!buf) return -1;
    memset(buf, 0, blocks_needed * fs->block_size);

    zirvfs_obj_header_t *hdr = (zirvfs_obj_header_t *)buf;
    hdr->type = data_size > 0 ? type : ZIRVFS_OBJ_TREE;
    hdr->data_size = (uint32_t)data_size;
    if (data_size > 0) {
        zirvfs_hash_data(data, data_size, &hdr->hash);
    } else {
        memset(&hdr->hash, 0, sizeof(hdr->hash));
    }

    if (data_size > 0)
        memcpy(buf + sizeof(zirvfs_obj_header_t), data, data_size);

    if (hash)
        zirvfs_hash_copy(hash, &hdr->hash);

    uint64_t first_block = 0;
    for (uint32_t b = 0; b < blocks_needed; b++) {
        uint64_t blk = zirvfs_alloc_block(fs);
        if (!blk) {
            kfree(buf);
            return -1;
        }
        if (b == 0) first_block = blk;
        zirvfs_write_block(fs, blk, buf + b * fs->block_size);

        if (b > 0) {
            uint64_t *next_ptr = (uint64_t *)(buf + (b-1) * fs->block_size + fs->block_size - 8);
            *next_ptr = blk;
            zirvfs_write_block(fs, first_block + b - 1, buf + (b-1) * fs->block_size);
        }
    }

    kfree(buf);

    if (block_out) *block_out = first_block;
    return 0;
}

int zirvfs_obj_read_by_block(zirvfs_t *fs, uint64_t block_no,
                              uint8_t *type_out, uint8_t *buf,
                              size_t buf_size, size_t *size_out)
{
    if (!block_no) return -1;

    zirvfs_obj_header_t hdr;
    if (zirvfs_read_block(fs, block_no, &hdr) < 0) return -1;

    uint32_t data_size = hdr.data_size;
    size_t total_size = sizeof(zirvfs_obj_header_t) + data_size;

    if (total_size > buf_size) {
        if (size_out) *size_out = total_size;
        return -2;
    }

    uint32_t blocks_needed = (uint32_t)((total_size + fs->block_size - 1) / fs->block_size);
    uint64_t *block_chain = (uint64_t *)kmalloc((blocks_needed + 1) * sizeof(uint64_t), 0);
    if (!block_chain) return -1;
    block_chain[0] = block_no;

    for (uint32_t b = 1; b < blocks_needed; b++) {
        uint8_t last_bytes[8];
        if (zirvfs_read_block(fs, block_chain[b-1], buf + (b-1) * fs->block_size) < 0) {
            kfree(block_chain);
            return -1;
        }
        memcpy(last_bytes, buf + (b-1) * fs->block_size + fs->block_size - 8, 8);
        block_chain[b] = *(uint64_t *)last_bytes;
        if (!block_chain[b]) {
            kfree(block_chain);
            return -1;
        }
    }

    if (blocks_needed > 1) {
        if (zirvfs_read_block(fs, block_chain[blocks_needed-1],
                               buf + (blocks_needed-1) * fs->block_size) < 0) {
            kfree(block_chain);
            return -1;
        }
    }

    kfree(block_chain);

    if (type_out) *type_out = hdr.type;
    if (size_out) *size_out = total_size;
    return 0;
}

int zirvfs_obj_read(zirvfs_t *fs, const zirvfs_hash_t *hash,
                     uint8_t *type_out, uint8_t *buf,
                     size_t buf_size, size_t *size_out)
{
    (void)fs;
    (void)hash;
    (void)type_out;
    (void)buf;
    (void)buf_size;
    (void)size_out;
    return -1;
}

int zirvfs_tree_create(zirvfs_t *fs, uint64_t *block_out, zirvfs_hash_t *hash)
{
    uint8_t empty = 0;
    return zirvfs_obj_write(fs, ZIRVFS_OBJ_TREE, &empty, 0, hash, block_out);
}

int zirvfs_tree_lookup(zirvfs_t *fs, uint64_t tree_block,
                        const char *name, zirvfs_tree_entry_t *entry)
{
    if (!tree_block || !name || !entry) return -1;

    size_t buf_size = fs->block_size * 4;
    uint8_t *buf = (uint8_t *)kmalloc(buf_size, 0);
    if (!buf) return -1;

    uint8_t obj_type;
    size_t obj_size;
    if (zirvfs_obj_read_by_block(fs, tree_block, &obj_type, buf, buf_size, &obj_size) < 0) {
        kfree(buf);
        return -1;
    }
    if (obj_type != ZIRVFS_OBJ_TREE) {
        kfree(buf);
        return -1;
    }

    uint32_t offset = sizeof(zirvfs_obj_header_t);
    uint32_t end = (obj_size < buf_size) ? (uint32_t)obj_size : (uint32_t)buf_size;

    while (offset + sizeof(zirvfs_tree_entry_t) <= end) {
        zirvfs_tree_entry_t *e = (zirvfs_tree_entry_t *)(buf + offset);
        if (e->name[0] == '\0') break;
        if (strcmp(e->name, name) == 0) {
            memcpy(entry, e, sizeof(zirvfs_tree_entry_t));
            kfree(buf);
            return 0;
        }
        offset += sizeof(zirvfs_tree_entry_t);
    }

    kfree(buf);
    return -1;
}

int zirvfs_tree_insert(zirvfs_t *fs, uint64_t *tree_block,
                        zirvfs_hash_t *tree_hash, const char *name,
                        uint32_t mode, const zirvfs_hash_t *item_hash)
{
    if (!tree_block || !name) return -1;

    size_t buf_size = fs->block_size * 8;
    uint8_t *buf = (uint8_t *)kmalloc(buf_size, 0);
    if (!buf) return -1;
    memset(buf, 0, buf_size);

    uint8_t obj_type;
    size_t obj_size = 0;

    if (*tree_block) {
        if (zirvfs_obj_read_by_block(fs, *tree_block, &obj_type, buf, buf_size, &obj_size) < 0) {
            obj_size = 0;
        }
    }

    uint32_t data_offset;
    if (obj_size > sizeof(zirvfs_obj_header_t)) {
        data_offset = sizeof(zirvfs_obj_header_t);
    } else {
        zirvfs_obj_header_t *hdr = (zirvfs_obj_header_t *)buf;
        hdr->type = ZIRVFS_OBJ_TREE;
        hdr->data_size = 0;
        memset(&hdr->hash, 0, sizeof(hdr->hash));
        data_offset = sizeof(zirvfs_obj_header_t);
        obj_size = sizeof(zirvfs_obj_header_t);
    }

    uint32_t end = (obj_size < buf_size) ? (uint32_t)obj_size : (uint32_t)buf_size;
    uint32_t offset = data_offset;
    int replaced = 0;

    while (offset + sizeof(zirvfs_tree_entry_t) <= end) {
        zirvfs_tree_entry_t *e = (zirvfs_tree_entry_t *)(buf + offset);
        if (e->name[0] == '\0') break;
        if (strcmp(e->name, name) == 0) {
            e->mode = mode;
            zirvfs_hash_copy(&e->hash, item_hash);
            replaced = 1;
            break;
        }
        offset += sizeof(zirvfs_tree_entry_t);
    }

    if (!replaced) {
        zirvfs_tree_entry_t *new_e = (zirvfs_tree_entry_t *)(buf + offset);
        new_e->mode = mode;
        size_t nlen = strlen(name);
        if (nlen > ZIRVFS_NAME_MAX) nlen = ZIRVFS_NAME_MAX;
        memcpy(new_e->name, name, nlen);
        new_e->name[nlen] = '\0';
        zirvfs_hash_copy(&new_e->hash, item_hash);
    }

    uint32_t new_data_end = offset + sizeof(zirvfs_tree_entry_t);
    zirvfs_obj_header_t *hdr = (zirvfs_obj_header_t *)buf;
    uint32_t new_data_size = new_data_end - sizeof(zirvfs_obj_header_t);
    hdr->data_size = new_data_size;

    zirvfs_hash_t new_hash;
    uint8_t *data_start = buf + sizeof(zirvfs_obj_header_t);
    zirvfs_hash_data(data_start, new_data_size, &new_hash);
    zirvfs_hash_copy(&hdr->hash, &new_hash);

    uint64_t old_block = *tree_block;

    uint64_t new_block;
    size_t total_size = sizeof(zirvfs_obj_header_t) + new_data_size;
    uint32_t blocks_needed = (uint32_t)((total_size + fs->block_size - 1) / fs->block_size);

    uint8_t *write_buf = (uint8_t *)kmalloc(blocks_needed * fs->block_size, 0);
    if (!write_buf) { kfree(buf); return -1; }
    memset(write_buf, 0, blocks_needed * fs->block_size);
    memcpy(write_buf, buf, total_size);

    new_block = 0;
    for (uint32_t b = 0; b < blocks_needed; b++) {
        uint64_t blk = zirvfs_alloc_block(fs);
        if (!blk) { kfree(write_buf); kfree(buf); return -1; }
        if (b == 0) new_block = blk;
        zirvfs_write_block(fs, blk, write_buf + b * fs->block_size);
        if (b > 0) {
            uint64_t *next_ptr = (uint64_t *)(write_buf + (b-1) * fs->block_size + fs->block_size - 8);
            *next_ptr = blk;
            zirvfs_write_block(fs, new_block + b - 1, write_buf + (b-1) * fs->block_size);
        }
    }

    kfree(write_buf);

    if (tree_hash) zirvfs_hash_copy(tree_hash, &new_hash);

    if (old_block)
        zirvfs_free_block(fs, old_block);

    *tree_block = new_block;
    fs->root_tree_block = new_block;

    kfree(buf);
    return 0;
}

int zirvfs_tree_remove(zirvfs_t *fs, uint64_t *tree_block,
                        zirvfs_hash_t *tree_hash, const char *name)
{
    if (!tree_block || !*tree_block || !name) return -1;

    size_t buf_size = fs->block_size * 8;
    uint8_t *buf = (uint8_t *)kmalloc(buf_size, 0);
    if (!buf) return -1;

    uint8_t obj_type;
    size_t obj_size;
    if (zirvfs_obj_read_by_block(fs, *tree_block, &obj_type, buf, buf_size, &obj_size) < 0) {
        kfree(buf);
        return -1;
    }
    if (obj_type != ZIRVFS_OBJ_TREE) { kfree(buf); return -1; }

    uint8_t *new_data = (uint8_t *)kmalloc(buf_size, 0);
    if (!new_data) { kfree(buf); return -1; }
    uint32_t new_offset = 0;

    uint32_t offset = sizeof(zirvfs_obj_header_t);
    uint32_t end = (obj_size < buf_size) ? (uint32_t)obj_size : (uint32_t)buf_size;
    int found = 0;

    while (offset + sizeof(zirvfs_tree_entry_t) <= end) {
        zirvfs_tree_entry_t *e = (zirvfs_tree_entry_t *)(buf + offset);
        if (e->name[0] == '\0') break;
        if (strcmp(e->name, name) != 0) {
            memcpy(new_data + new_offset, e, sizeof(zirvfs_tree_entry_t));
            new_offset += sizeof(zirvfs_tree_entry_t);
        } else {
            found = 1;
        }
        offset += sizeof(zirvfs_tree_entry_t);
    }

    if (!found) { kfree(new_data); kfree(buf); return -1; }

    uint64_t old_block = *tree_block;
    zirvfs_hash_t new_hash;
    zirvfs_hash_data(new_data, new_offset, &new_hash);

    uint64_t new_block;
    zirvfs_obj_write(fs, ZIRVFS_OBJ_TREE, new_data, new_offset, &new_hash, &new_block);

    kfree(new_data);
    kfree(buf);

    if (old_block) zirvfs_free_block(fs, old_block);

    *tree_block = new_block;
    if (tree_hash) zirvfs_hash_copy(tree_hash, &new_hash);
    fs->root_tree_block = new_block;
    return 0;
}

int zirvfs_tree_readdir(zirvfs_t *fs, uint64_t tree_block,
                         uint32_t idx, char *name_out, size_t namelen)
{
    if (!tree_block || !name_out) return -1;

    size_t buf_size = fs->block_size * 4;
    uint8_t *buf = (uint8_t *)kmalloc(buf_size, 0);
    if (!buf) return -1;

    uint8_t obj_type;
    size_t obj_size;
    if (zirvfs_obj_read_by_block(fs, tree_block, &obj_type, buf, buf_size, &obj_size) < 0) {
        kfree(buf);
        return -1;
    }
    if (obj_type != ZIRVFS_OBJ_TREE) { kfree(buf); return -1; }

    uint32_t offset = sizeof(zirvfs_obj_header_t);
    uint32_t end = (obj_size < buf_size) ? (uint32_t)obj_size : (uint32_t)buf_size;
    uint32_t count = 0;

    while (offset + sizeof(zirvfs_tree_entry_t) <= end) {
        zirvfs_tree_entry_t *e = (zirvfs_tree_entry_t *)(buf + offset);
        if (e->name[0] == '\0') break;
        if (count == idx) {
            size_t nlen = strlen(e->name);
            if (nlen >= namelen) nlen = namelen - 1;
            memcpy(name_out, e->name, nlen);
            name_out[nlen] = '\0';
            kfree(buf);
            return 0;
        }
        count++;
        offset += sizeof(zirvfs_tree_entry_t);
    }

    kfree(buf);
    return -1;
}

static int zirvfs_vfs_read(vnode_t *v, void *buf, size_t len, uint64_t off)
{
    if (!v || !v->private_data) return -1;
    zirvfs_tree_entry_t *entry = (zirvfs_tree_entry_t *)v->private_data;

    zirvfs_t *fs = (zirvfs_t *)v->parent->private_data;
    if (!fs) return -1;

    uint64_t hash64 = 0;
    for (int i = 0; i < 8; i++)
        hash64 = (hash64 << 8) | entry->hash.bytes[i];
    uint64_t block_no = (hash64 % (fs->total_blocks - fs->num_bitmap_blocks - 1))
                        + fs->num_bitmap_blocks + 1;

    size_t buf_size = fs->block_size * 4;
    uint8_t *obj_buf = (uint8_t *)kmalloc(buf_size, 0);
    if (!obj_buf) return -1;

    uint8_t obj_type;
    size_t obj_size;
    if (zirvfs_obj_read_by_block(fs, block_no, &obj_type, obj_buf, buf_size, &obj_size) < 0) {
        kfree(obj_buf);
        return -1;
    }

    uint32_t data_offset = sizeof(zirvfs_obj_header_t);
    if (off >= obj_size - data_offset) { kfree(obj_buf); return 0; }
    size_t avail = (obj_size - data_offset) - (size_t)off;
    if (len > avail) len = avail;
    memcpy(buf, obj_buf + data_offset + (size_t)off, len);
    kfree(obj_buf);
    return (int)len;
}

static int zirvfs_vfs_write(vnode_t *v, const void *buf, size_t len, uint64_t off)
{
    (void)v; (void)buf; (void)len; (void)off;
    return -1;
}

static int zirvfs_vfs_readdir(vnode_t *dir, uint32_t idx,
                               char *name_out, size_t namelen)
{
    if (!dir || dir->type != VNODE_DIR) return -1;
    if (!dir->private_data) return -1;
    zirvfs_t *fs = (zirvfs_t *)dir->private_data;

    if (!fs->root_tree_block) return -1;
    return zirvfs_tree_readdir(fs, fs->root_tree_block, idx, name_out, namelen);
}

static vnode_t *zirvfs_vfs_lookup(vnode_t *dir, const char *name)
{
    if (!dir || !name) return NULL;
    if (!dir->private_data) return NULL;
    zirvfs_t *fs = (zirvfs_t *)dir->private_data;
    if (!fs->root_tree_block) return NULL;

    zirvfs_tree_entry_t entry;
    if (zirvfs_tree_lookup(fs, fs->root_tree_block, name, &entry) < 0)
        return NULL;

    vnode_t *v = (vnode_t *)kmalloc(sizeof(vnode_t), 0);
    if (!v) return NULL;
    memset(v, 0, sizeof(vnode_t));
    size_t nlen = strlen(name);
    if (nlen > VNODE_NAME_MAX) nlen = VNODE_NAME_MAX;
    memcpy(v->name, name, nlen);
    v->name[nlen] = '\0';

    zirvfs_tree_entry_t *entry_copy = (zirvfs_tree_entry_t *)kmalloc(sizeof(zirvfs_tree_entry_t), 0);
    if (!entry_copy) { kfree(v); return NULL; }
    memcpy(entry_copy, &entry, sizeof(zirvfs_tree_entry_t));
    v->private_data = entry_copy;

    if (entry.mode & 0x4000) {
        v->type = VNODE_DIR;
        v->ops   = &(const vnode_ops_t){
            .readdir = zirvfs_vfs_readdir,
            .lookup  = zirvfs_vfs_lookup,
        };
    } else {
        v->type = VNODE_FILE;
        v->ops   = &(const vnode_ops_t){
            .read  = zirvfs_vfs_read,
            .write = zirvfs_vfs_write,
        };
    }

    v->parent = dir;
    v->permissions = MOSIX_PERM_READ | MOSIX_PERM_EXEC;
    return v;
}

static int zirvfs_vfs_create(vnode_t *dir, const char *name, vnode_type_t type)
{
    (void)dir; (void)name; (void)type;
    return -1;
}

static int zirvfs_vfs_unlink(vnode_t *dir, const char *name)
{
    (void)dir; (void)name;
    return -1;
}

static void zirvfs_vfs_release(vnode_t *v)
{
    if (v && v->private_data) {
        kfree(v->private_data);
        v->private_data = NULL;
    }
}

static const vnode_ops_t zirvfs_dir_ops = {
    .readdir = zirvfs_vfs_readdir,
    .lookup  = zirvfs_vfs_lookup,
    .create  = zirvfs_vfs_create,
    .unlink  = zirvfs_vfs_unlink,
    .release = zirvfs_vfs_release,
};

static void int_to_str(uint32_t n, char *buf, size_t bufsize)
{
    char tmp[16];
    int i = 0;
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (n > 0 && i < (int)sizeof(tmp) - 1) {
        tmp[i++] = (char)('0' + n % 10);
        n /= 10;
    }
    int j = 0;
    while (i > 0 && j < (int)bufsize - 1)
        buf[j++] = tmp[--i];
    buf[j] = '\0';
}

static int zirvfs_mount_instance(zirvfs_t *fs)
{
    if (!fs || fs->mounted) return -1;

    zirvfs_superblock_t sb;
    if (zirvfs_read_block(fs, ZIRVFS_SB_BLOCK, &sb) < 0) return -1;

    if (memcmp(sb.magic, ZIRVFS_MAGIC, 8) != 0) {
        klog(LOG_INFO, "ZFS", "No ZirvFS superblock found on device");
        return -1;
    }

    fs->block_size = sb.block_size;
    fs->total_blocks = sb.total_blocks;
    fs->num_bitmap_blocks = sb.bitmap_blocks;
    fs->bitmap_block = sb.allocation_bitmap_blk;
    fs->free_count = sb.free_count;
    fs->root_tree_block = sb.root_tree_blk;
    fs->root_commit_block = sb.root_commit_blk;
    fs->commit_count = sb.commit_count;
    fs->formatted = true;

    if (fs->block_size == 4096) fs->block_shift = 12;
    else if (fs->block_size == 16384) fs->block_shift = 14;
    else if (fs->block_size == 65536) fs->block_shift = 16;

    vnode_t *root = (vnode_t *)kmalloc(sizeof(vnode_t), 0);
    if (!root) return -1;
    memset(root, 0, sizeof(vnode_t));
    root->name[0] = '/';
    root->name[1] = '\0';
    root->type = VNODE_DIR;
    root->permissions = MOSIX_PERM_READ | MOSIX_PERM_EXEC;
    root->ops = &zirvfs_dir_ops;
    root->private_data = fs;
    fs->root_vnode = root;

    fs->vfs.type_name = "zirvfs";
    fs->vfs.root = root;
    fs->vfs.fs_data = fs;

    char mount_path[64];
    memcpy(mount_path, "/mounts/zirvfs", 14);
    int_to_str(zirvfs_instance_count, mount_path + 14, sizeof(mount_path) - 14);

    vnode_t *mounts = vfs_lookup("/mounts");
    if (!mounts) {
        vfs_mkdir("/mounts");
    }

    vfs_mkdir(mount_path);
    vfs_mount(mount_path, &fs->vfs);

    fs->mounted = true;
    klog(LOG_OK, "ZFS", "Mounted at %s (block_size=%u, %llu blocks, %llu free)",
         mount_path, fs->block_size,
         (unsigned long long)fs->total_blocks,
         (unsigned long long)fs->free_count);

    return 0;
}

void zirvfs_init(void)
{
    klog(LOG_INFO, "ZFS", "Initialising ZirvFS CoW+Git filesystem");

    uint32_t dev_count = 0;

    for (uint32_t i = 0; i < device_count_total; i++) {
        registered_device_t *reg = &device_pool[i];
        device_desc_t *desc = &reg->desc;

        if (desc->bus_class == DEV_CLASS_SATA ||
            desc->bus_class == DEV_CLASS_PATA ||
            desc->bus_class == DEV_CLASS_NVME ||
            desc->bus_class == DEV_CLASS_USB) {

            if (desc->media_class != DEV_CLASS_HDD &&
                desc->media_class != DEV_CLASS_SSD &&
                desc->media_class != DEV_CLASS_PENDRIVE)
                continue;

            zirvfs_t *fs = (zirvfs_t *)kmalloc(sizeof(zirvfs_t), 0);
            if (!fs) continue;
            memset(fs, 0, sizeof(zirvfs_t));

            fs->device = reg;
            fs->dev_ops = reg->ops;
            fs->desc = desc;
            fs->block_size = zirvfs_detect_block_size(desc->media_class);

            if (fs->block_size == 4096) fs->block_shift = 12;
            else if (fs->block_size == 16384) fs->block_shift = 14;
            else if (fs->block_size == 65536) fs->block_shift = 16;

            zirvfs_superblock_t sb;
            int has_sb = zirvfs_read_block(fs, ZIRVFS_SB_BLOCK, &sb);
            if (has_sb == 0 && memcmp(sb.magic, ZIRVFS_MAGIC, 8) == 0) {
                klog(LOG_INFO, "ZFS", "Found existing ZirvFS on %s (%s)",
                     desc->model,
                     desc->media_class == DEV_CLASS_SSD ? "SSD" :
                     desc->media_class == DEV_CLASS_HDD ? "HDD" : "Pendrive");
                zirvfs_mount_instance(fs);
            } else {
                klog(LOG_INFO, "ZFS", "Formatting %s (%s, block_size=%u)",
                     desc->model,
                     desc->media_class == DEV_CLASS_SSD ? "SSD" :
                     desc->media_class == DEV_CLASS_HDD ? "HDD" : "Pendrive",
                     fs->block_size);
                if (zirvfs_format(fs) == 0)
                    zirvfs_mount_instance(fs);
            }

            fs->next = zirvfs_instances;
            zirvfs_instances = fs;
            zirvfs_instance_count++;
            dev_count++;
        }
    }

    klog(LOG_OK, "ZFS", "%u ZirvFS volumes initialised on storage devices",
         dev_count);
}
