#include "mosix.h"
#include "kernel/mm/pmm.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

extern void *kmalloc(size_t size, unsigned int flags);
extern void kfree(void *ptr);

#define TMPFS_BLOCK 4096

typedef struct tmpfs_file {
    uint8_t *data;
    uint64_t size;
    uint64_t capacity;
} tmpfs_file_t;

static vnode_t *tmpfs_root = NULL;
static uint64_t tmpfs_next_ino = 1000;

static uint64_t tmpfs_alloc_ino(void) { return tmpfs_next_ino++; }

static tmpfs_file_t *tmpfs_file_create(void) {
    tmpfs_file_t *f = (tmpfs_file_t *)kmalloc(sizeof(tmpfs_file_t), 0);
    if (!f) return NULL;
    f->data = NULL;
    f->size = 0;
    f->capacity = 0;
    return f;
}

static void tmpfs_file_destroy(tmpfs_file_t *f) {
    if (!f) return;
    if (f->data) kfree(f->data);
    kfree(f);
}

static int tmpfs_file_write(tmpfs_file_t *f, const void *buf, size_t len, uint64_t off) {
    uint64_t needed = off + len;
    if (needed > f->capacity) {
        uint64_t new_cap = f->capacity ? f->capacity : TMPFS_BLOCK;
        while (new_cap < needed) new_cap *= 2;
        uint8_t *new_data = (uint8_t *)kmalloc(new_cap, 0);
        if (!new_data) return -1;
        if (f->data) {
            memcpy(new_data, f->data, f->size);
            kfree(f->data);
        }
        f->data = new_data;
        f->capacity = new_cap;
    }
    memcpy(f->data + off, buf, len);
    if (off + len > f->size) f->size = off + len;
    return (int)len;
}

static int tmpfs_file_read(tmpfs_file_t *f, void *buf, size_t len, uint64_t off) {
    if (off >= f->size) return 0;
    size_t avail = f->size - off;
    if (len > avail) len = avail;
    memcpy(buf, f->data + off, len);
    return (int)len;
}

static vnode_t *tmpfs_lookup(vnode_t *dir, const char *name) {
    for (vnode_t *c = dir->children; c; c = c->next_sibling)
        if (strcmp(c->name, name) == 0) return c;
    return NULL;
}

static int tmpfs_readdir(vnode_t *dir, uint32_t idx, char *name_out, size_t namelen) {
    vnode_t *c = dir->children;
    for (uint32_t i = 0; c && i < idx; i++) c = c->next_sibling;
    if (!c) return -1;
    size_t n = strlen(c->name);
    if (n >= namelen) n = namelen - 1;
    memcpy(name_out, c->name, n);
    name_out[n] = '\0';
    return 0;
}

static int tmpfs_read(vnode_t *v, void *buf, size_t len, uint64_t off) {
    tmpfs_file_t *f = (tmpfs_file_t *)v->private_data;
    if (!f) return 0;
    return tmpfs_file_read(f, buf, len, off);
}

static int tmpfs_write(vnode_t *v, const void *buf, size_t len, uint64_t off) {
    tmpfs_file_t *f = (tmpfs_file_t *)v->private_data;
    if (!f) {
        f = tmpfs_file_create();
        if (!f) return -1;
        v->private_data = f;
    }
    int n = tmpfs_file_write(f, buf, len, off);
    if (n > 0) v->size = f->size;
    return n;
}

const vnode_ops_t tmpfs_dir_ops;
const vnode_ops_t tmpfs_file_ops;

static int tmpfs_create(vnode_t *dir, const char *name, vnode_type_t type) {
    if (!dir || dir->type != VNODE_DIR) return -1;
    if (tmpfs_lookup(dir, name)) return -1;
    vnode_t *v = (vnode_t *)kmalloc(sizeof(vnode_t), 0);
    if (!v) return -1;
    memset(v, 0, sizeof(vnode_t));
    size_t nlen = strlen(name);
    if (nlen > 255) nlen = 255;
    memcpy(v->name, name, nlen);
    v->name[nlen] = '\0';
    v->type = type;
    v->inode = tmpfs_alloc_ino();
    v->permissions = MOSIX_PERM_READ | MOSIX_PERM_WRITE | MOSIX_PERM_EXEC;
    v->parent = dir;
    vnode_t *s = dir->children;
    if (!s) { dir->children = v; }
    else { while (s->next_sibling) s = s->next_sibling; s->next_sibling = v; }
    if (type == VNODE_FILE) {
        v->ops = &tmpfs_file_ops;
    } else {
        v->ops = &tmpfs_dir_ops;
    }
    return 0;
}

static int tmpfs_unlink(vnode_t *dir, const char *name) {
    if (!dir || dir->type != VNODE_DIR) return -1;
    vnode_t *prev = NULL;
    vnode_t *cur = dir->children;
    while (cur) {
        if (strcmp(cur->name, name) == 0) {
            if (cur->type == VNODE_DIR && cur->children) return -1;
            if (prev) prev->next_sibling = cur->next_sibling;
            else dir->children = cur->next_sibling;
            if (cur->private_data) tmpfs_file_destroy((tmpfs_file_t *)cur->private_data);
            kfree(cur);
            return 0;
        }
        prev = cur;
        cur = cur->next_sibling;
    }
    return -1;
}

const vnode_ops_t tmpfs_dir_ops = {
    .readdir = tmpfs_readdir,
    .lookup = tmpfs_lookup,
    .create = tmpfs_create,
    .unlink = tmpfs_unlink,
};

const vnode_ops_t tmpfs_file_ops = {
    .read = tmpfs_read,
    .write = tmpfs_write,
};

static vfs_t tmpfs_vfs = {
    .type_name = "tmpfs",
    .root = NULL,
    .fs_data = NULL,
};

void tmpfs_init(void) {
    tmpfs_root = (vnode_t *)kmalloc(sizeof(vnode_t), 0);
    if (!tmpfs_root) return;
    memset(tmpfs_root, 0, sizeof(vnode_t));
    tmpfs_root->name[0] = '/';
    tmpfs_root->name[1] = '\0';
    tmpfs_root->type = VNODE_DIR;
    tmpfs_root->inode = tmpfs_alloc_ino();
    tmpfs_root->permissions = MOSIX_PERM_READ | MOSIX_PERM_WRITE | MOSIX_PERM_EXEC;
    tmpfs_root->ops = &tmpfs_dir_ops;
    tmpfs_vfs.root = tmpfs_root;
    vfs_mount("/tmp", &tmpfs_vfs);

    /* Mount a second tmpfs at /home for liveCD computing platform */
    vnode_t *home_root = (vnode_t *)kmalloc(sizeof(vnode_t), 0);
    if (home_root) {
        memset(home_root, 0, sizeof(vnode_t));
        home_root->name[0] = '/';
        home_root->name[1] = '\0';
        home_root->type = VNODE_DIR;
        home_root->inode = tmpfs_alloc_ino();
        home_root->permissions = MOSIX_PERM_READ | MOSIX_PERM_WRITE | MOSIX_PERM_EXEC;
        home_root->ops = &tmpfs_dir_ops;
        static vfs_t home_vfs = {
            .type_name = "tmpfs",
            .root = NULL,
            .fs_data = NULL,
        };
        home_vfs.root = home_root;
        vfs_mount("/home", &home_vfs);
    }

    /* Mount /var/run for runtime state */
    vnode_t *var_root = (vnode_t *)kmalloc(sizeof(vnode_t), 0);
    if (var_root) {
        memset(var_root, 0, sizeof(vnode_t));
        var_root->name[0] = '/';
        var_root->name[1] = '\0';
        var_root->type = VNODE_DIR;
        var_root->inode = tmpfs_alloc_ino();
        var_root->permissions = MOSIX_PERM_READ | MOSIX_PERM_WRITE | MOSIX_PERM_EXEC;
        var_root->ops = &tmpfs_dir_ops;
        static vfs_t var_vfs = {
            .type_name = "tmpfs",
            .root = NULL,
            .fs_data = NULL,
        };
        var_vfs.root = var_root;
        vfs_mount("/var", &var_vfs);
    }
}
