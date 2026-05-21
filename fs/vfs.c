/* fs/vfs.c
 * Zirvium Kernel — MOSIX VFS implementation
 *
 * Provides the in-memory virtual filesystem tree that enforces the MOSIX
 * directory hierarchy and the /zirv device namespace.
 */
#include "mosix.h"
#include "drivers/serial/serial.h"
#include "drivers/zirv/input/ps2/keyboard.h"
#include "arch/x64/cpu.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

extern void *kmalloc(size_t size, unsigned int flags);

/* ── Simple kernel memory allocation shim ────────────────────────────────── */
/* A very small slab-style bump allocator seeded from a static arena.
 * The real allocator (kernel heap) will replace this once mm is fully up. */
#define VFS_ARENA_SIZE  (256 * 1024)   /* 256 KiB */
static uint8_t  vfs_arena[VFS_ARENA_SIZE];
static size_t   vfs_arena_off = 0;

static void *vfs_alloc(size_t sz)
{
    /* Align to 16 bytes */
    sz = (sz + 15) & ~(size_t)15;
    if (vfs_arena_off + sz > VFS_ARENA_SIZE)
        return NULL;
    void *p = vfs_arena + vfs_arena_off;
    vfs_arena_off += sz;
    memset(p, 0, sz);
    return p;
}

/* ── Top-level MOSIX directory names ──────────────────────────────────────── */
const char *mosix_top_dirs[MOSIX_TOP_DIRS] = {
    "bin", "lib", "user", "boot", "config", "zirv", "mounts", "tmp"
};

/* ── /zirv sub-directory names ────────────────────────────────────────────── */
static const char *zirv_bus_dirs[] = {
    "sata", "pata", "nvme", "usb", "net", "input", "display", "tty", "audio"
};
#define ZIRV_BUS_COUNT  (sizeof(zirv_bus_dirs) / sizeof(zirv_bus_dirs[0]))

/* ── Bus-class → sub-directory mapping ────────────────────────────────────── */
static const char *bus_class_name(dev_class_t bc)
{
    switch (bc) {
    case DEV_CLASS_SATA:  return "sata";
    case DEV_CLASS_PATA:  return "pata";
    case DEV_CLASS_NVME:  return "nvme";
    case DEV_CLASS_USB:   return "usb";
    case DEV_CLASS_NET_ETH:
    case DEV_CLASS_NET_WLAN:
    case DEV_CLASS_NET_LOOPBACK: return "net";
    case DEV_CLASS_INPUT_KEYBOARD:
    case DEV_CLASS_INPUT_MOUSE:
    case DEV_CLASS_INPUT_TOUCHPAD: return "input";
    case DEV_CLASS_DISPLAY_FB:
    case DEV_CLASS_DISPLAY_GPU:    return "display";
    case DEV_CLASS_TTY_SERIAL:
    case DEV_CLASS_TTY_VIRTUAL:    return "tty";
    case DEV_CLASS_AUDIO_OUTPUT:
    case DEV_CLASS_AUDIO_INPUT:    return "audio";
    default: return "unknown";
    }
}

static const char *media_class_name(dev_class_t mc)
{
    switch (mc) {
    case DEV_CLASS_HDD:          return "hdd";
    case DEV_CLASS_SSD:          return "ssd";
    case DEV_CLASS_CDROM:        return "cdrom";
    case DEV_CLASS_CDRW:         return "cdrw";
    case DEV_CLASS_DVDROM:       return "dvdrom";
    case DEV_CLASS_DVDRW:        return "dvdrw";
    case DEV_CLASS_PENDRIVE:     return "pendrive";
    case DEV_CLASS_NET_ETH:      return "eth";
    case DEV_CLASS_NET_WLAN:     return "wlan";
    case DEV_CLASS_NET_LOOPBACK: return "lo";
    case DEV_CLASS_INPUT_KEYBOARD: return "keyboard";
    case DEV_CLASS_INPUT_MOUSE:    return "mouse";
    case DEV_CLASS_INPUT_TOUCHPAD: return "touchpad";
    case DEV_CLASS_DISPLAY_FB:   return "framebuffer";
    case DEV_CLASS_DISPLAY_GPU:  return "gpu";
    case DEV_CLASS_TTY_SERIAL:   return "serial";
    case DEV_CLASS_TTY_VIRTUAL:  return "virtual";
    case DEV_CLASS_AUDIO_OUTPUT: return "output";
    case DEV_CLASS_AUDIO_INPUT:  return "input";
    default: return "unknown";
    }
}

/* ── VNode constructor ────────────────────────────────────────────────────── */
static vnode_t *make_vnode(const char *name, vnode_type_t type,
                           uint8_t perms, vnode_t *parent)
{
    vnode_t *v = (vnode_t *)vfs_alloc(sizeof(vnode_t));
    if (!v) return NULL;

    size_t nlen = strlen(name);
    if (nlen > VNODE_NAME_MAX) nlen = VNODE_NAME_MAX;
    memcpy(v->name, name, nlen);
    v->name[nlen] = '\0';

    v->type        = type;
    v->permissions = perms;
    v->parent      = parent;

    /* Link into parent's children list */
    if (parent) {
        if (!parent->children) {
            parent->children = v;
        } else {
            vnode_t *s = parent->children;
            while (s->next_sibling) s = s->next_sibling;
            s->next_sibling = v;
        }
    }
    return v;
}

/* ── Static inode counter ─────────────────────────────────────────────────── */
static uint64_t next_inode = 1;
static uint64_t alloc_inode(void) { return next_inode++; }

/* ── Root vnode ───────────────────────────────────────────────────────────── */
static vnode_t root_vnode;
static bool vfs_initialised = false;

/* ── Default ops for read-only directory nodes ────────────────────────────── */
static int dir_readdir(vnode_t *dir, uint32_t idx,
                       char *name_out, size_t namelen)
{
    vnode_t *child = dir->children;
    for (uint32_t i = 0; child && i < idx; i++)
        child = child->next_sibling;
    if (!child) return -1;
    size_t n = strlen(child->name);
    if (n >= namelen) n = namelen - 1;
    memcpy(name_out, child->name, n);
    name_out[n] = '\0';
    return 0;
}

static vnode_t *dir_lookup(vnode_t *dir, const char *name)
{
    for (vnode_t *c = dir->children; c; c = c->next_sibling) {
        if (strcmp(c->name, name) == 0)
            return c;
    }
    return NULL;
}

static int dir_create(vnode_t *dir, const char *name, vnode_type_t type);

static int dir_unlink(vnode_t *dir, const char *name);

static const vnode_ops_t dir_ops = {
    .read    = NULL,
    .write   = NULL,
    .readdir = dir_readdir,
    .lookup  = dir_lookup,
    .create  = dir_create,
    .unlink  = dir_unlink,
    .release = NULL,
};

static int dir_create(vnode_t *dir, const char *name, vnode_type_t type)
{
    if (!dir || dir->type != VNODE_DIR) return -1;
    if (!name || !*name) return -1;
    if (dir_lookup(dir, name)) return -1;

    uint8_t perms = MOSIX_PERM_READ | MOSIX_PERM_WRITE | MOSIX_PERM_EXEC;
    vnode_t *v = make_vnode(name, type, perms, dir);
    if (!v) return -1;
    v->inode = alloc_inode();
    v->ops   = &dir_ops;
    return 0;
}

static int dir_unlink(vnode_t *dir, const char *name)
{
    if (!dir || dir->type != VNODE_DIR) return -1;
    if (!name || !*name) return -1;

    vnode_t *prev = NULL;
    vnode_t *cur  = dir->children;
    while (cur) {
        if (strcmp(cur->name, name) == 0) {
            if (cur->type == VNODE_DIR && cur->children)
                return -1;
            if (prev)
                prev->next_sibling = cur->next_sibling;
            else
                dir->children = cur->next_sibling;
            return 0;
        }
        prev = cur;
        cur  = cur->next_sibling;
    }
    return -1;
}

/* ── Console Device Ops ──────────────────────────────────────────────────── */
extern void kputs(const char *s);

static int console_write_vfs(vnode_t *vn, const void *buf, size_t count, uint64_t off)
{
    (void)vn; (void)off;
    const char *cbuf = (const char *)buf;
    for (size_t i = 0; i < count; i++) {
        char s[2] = {cbuf[i], 0};
        kputs(s);
    }
    return (int)count;
}

static int console_read_vfs(vnode_t *vn, void *buf, size_t count, uint64_t off)
{
    (void)vn; (void)off;
    char *cbuf = (char *)buf;
    for (size_t i = 0; i < count; i++) {
        char c = 0;
        for (;;) {
            int kc = keyboard_read_ascii();
            if (kc > 0) { c = (char)kc; break; }
            if (serial_available(SERIAL_COM1)) { c = serial_getc(SERIAL_COM1); break; }
            __asm__ volatile("sti; pause; cli");
        }
        cbuf[i] = c;
    }
    return (int)count;
}

static const vnode_ops_t console_vnode_ops = {
    .read  = console_read_vfs,
    .write = console_write_vfs,
};

/* ── VFS public API ───────────────────────────────────────────────────────── */

void vfs_init(void)
{
    if (vfs_initialised) return;
    vfs_initialised = true;

    /* Initialise root "/" */
    memset(&root_vnode, 0, sizeof(root_vnode));
    root_vnode.name[0]     = '/';
    root_vnode.name[1]     = '\0';
    root_vnode.type        = VNODE_DIR;
    root_vnode.permissions = MOSIX_PERM_READ | MOSIX_PERM_EXEC;
    root_vnode.inode       = alloc_inode();
    root_vnode.ops         = &dir_ops;

    /* Create top-level MOSIX directories */
    for (int i = 0; i < MOSIX_TOP_DIRS; i++) {
        uint8_t perms = MOSIX_PERM_READ | MOSIX_PERM_EXEC;
        /* /tmp and /mounts are writable by all */
        if (strcmp(mosix_top_dirs[i], "tmp")    == 0 ||
            strcmp(mosix_top_dirs[i], "mounts") == 0)
            perms |= MOSIX_PERM_WRITE;

        vnode_t *d = make_vnode(mosix_top_dirs[i], VNODE_DIR, perms, &root_vnode);
        if (d) {
            d->inode = alloc_inode();
            d->ops   = &dir_ops;
        }
    }

    /* Populate /zirv with bus sub-directories */
    vnode_t *zirv = dir_lookup(&root_vnode, "zirv");
    if (!zirv) return;

    for (size_t b = 0; b < ZIRV_BUS_COUNT; b++) {
        vnode_t *busdir = make_vnode(zirv_bus_dirs[b], VNODE_DIR,
                                     MOSIX_PERM_READ | MOSIX_PERM_EXEC,
                                     zirv);
        if (busdir) {
            busdir->inode = alloc_inode();
            busdir->ops   = &dir_ops;
        }
    }

    /* Create /zirv/tty/virtual0 (The system console) */
    vnode_t *tty_dir = vfs_lookup("/zirv/tty");
    if (tty_dir) {
        vnode_t *con = make_vnode("virtual0", VNODE_DEVICE,
                                  MOSIX_PERM_READ | MOSIX_PERM_WRITE, tty_dir);
        if (con) {
            con->inode = alloc_inode();
            con->ops   = &console_vnode_ops;
        }
    }
}

vnode_t *vfs_root(void)
{
    return &root_vnode;
}

vnode_t *vfs_lookup(const char *path)
{
    if (!path || path[0] != '/') return NULL;

    vnode_t *cur = &root_vnode;
    const char *p = path + 1;   /* skip leading '/' */

    while (*p) {
        /* Extract next path component */
        char component[VNODE_NAME_MAX + 1];
        size_t len = 0;
        while (*p && *p != '/' && len < VNODE_NAME_MAX)
            component[len++] = *p++;
        component[len] = '\0';
        if (*p == '/') p++;

        if (len == 0) continue;   /* skip double slashes */

        if (cur->type == VNODE_MOUNTPT && cur->mounted_fs)
            cur = cur->mounted_fs->root;

        if (cur->type != VNODE_DIR && cur->type != VNODE_MOUNTPT)
            return NULL;

        if (!cur->ops || !cur->ops->lookup) return NULL;
        cur = cur->ops->lookup(cur, component);
        if (!cur) return NULL;
    }
    return cur;
}

int vfs_mount(const char *path, vfs_t *fs)
{
    if (!path || !fs) return -1;
    vnode_t *mp = vfs_lookup(path);
    if (!mp) return -2;
    if (mp->type != VNODE_DIR && mp->type != VNODE_MOUNTPT) return -3;
    mp->type       = VNODE_MOUNTPT;
    mp->mounted_fs = fs;
    return 0;
}

vnode_t *vfs_register_device(dev_class_t bus_class, dev_class_t media_class,
                              uint32_t index, device_desc_t *desc)
{
    if (!vfs_initialised) return NULL;

    /* Locate /zirv/<bus>/ */
    vnode_t *zirv = vfs_lookup("/zirv");
    if (!zirv) return NULL;

    const char *bname = bus_class_name(bus_class);
    vnode_t *busdir = dir_lookup(zirv, bname);
    if (!busdir) {
        busdir = make_vnode(bname, VNODE_DIR,
                            MOSIX_PERM_READ | MOSIX_PERM_EXEC, zirv);
        if (!busdir) return NULL;
        busdir->inode = alloc_inode();
        busdir->ops   = &dir_ops;
    }

    /* Build leaf name: e.g. "hdd0", "ssd1" */
    const char *mname = media_class_name(media_class);
    char leaf[VNODE_NAME_MAX + 1];
    /* Simple number-to-string suffix */
    size_t nlen = strlen(mname);
    memcpy(leaf, mname, nlen);
    /* Append decimal index */
    uint32_t idx = index;
    char numbuf[12];
    int ndigits = 0;
    if (idx == 0) {
        numbuf[ndigits++] = '0';
    } else {
        uint32_t tmp = idx;
        while (tmp) { numbuf[ndigits++] = (char)('0' + tmp % 10); tmp /= 10; }
        /* Reverse */
        for (int i = 0; i < ndigits / 2; i++) {
            char t = numbuf[i]; numbuf[i] = numbuf[ndigits - 1 - i];
            numbuf[ndigits - 1 - i] = t;
        }
    }
    memcpy(leaf + nlen, numbuf, (size_t)ndigits);
    leaf[nlen + (size_t)ndigits] = '\0';

    /* Create the device vnode */
    vnode_t *devnode = make_vnode(leaf, VNODE_DEVICE,
                                  MOSIX_PERM_READ | MOSIX_PERM_WRITE, busdir);
    if (!devnode) return NULL;
    devnode->inode  = alloc_inode();
    devnode->device = desc;

    return devnode;
}

/* ── Path helper: split "/parent/path/leaf" into parent_path + leaf ──────── */
/* Returns leaf name pointer within path (mutates path: replaces '/' with '\0') */
static const char *split_parent_path(char *path, char **parent_out)
{
    if (!path || path[0] != '/') return NULL;
    char *leaf = NULL;
    char *p = path;
    while (*p) {
        if (*p == '/') {
            if (*(p+1) != '\0')
                leaf = p + 1;
        }
        p++;
    }
    if (!leaf || !*leaf) return NULL;
    /* terminate parent path: overwrite '/' before leaf */
    *(leaf - 1) = '\0';
    if (leaf - 1 == path) {
        /* parent is root: restore the '/' so path reads "/" */
        *path = '/';
        *(path + 1) = '\0';
        *parent_out = path;
    } else {
        *parent_out = path;
    }
    return leaf;
}

/* ── Public VFS API: mkdir, rmdir, unlink, rename ────────────────────────── */

int vfs_mkdir(const char *path)
{
    if (!path || path[0] != '/') return -1;

    char buf[1024];
    size_t plen = strlen(path);
    if (plen >= sizeof(buf)) return -1;
    memcpy(buf, path, plen + 1);

    char *parent_path;
    const char *leaf = split_parent_path(buf, &parent_path);
    if (!leaf) return -1;

    vnode_t *parent = vfs_lookup(parent_path);
    if (!parent || parent->type != VNODE_DIR) return -1;
    if (!parent->ops || !parent->ops->create) return -1;

    return parent->ops->create(parent, leaf, VNODE_DIR);
}

int vfs_rmdir(const char *path)
{
    if (!path || path[0] != '/') return -1;

    char buf[1024];
    size_t plen = strlen(path);
    if (plen >= sizeof(buf)) return -1;
    memcpy(buf, path, plen + 1);

    char *parent_path;
    const char *leaf = split_parent_path(buf, &parent_path);
    if (!leaf) return -1;

    vnode_t *parent = vfs_lookup(parent_path);
    if (!parent || parent->type != VNODE_DIR) return -1;
    if (!parent->ops || !parent->ops->unlink) return -1;

    return parent->ops->unlink(parent, leaf);
}

int vfs_unlink(const char *path)
{
    return vfs_rmdir(path);
}

/* ── Helper: extract leaf name from a path (last component) ─────────────── */
static const char *path_leaf(const char *path)
{
    if (!path || !*path) return NULL;
    const char *leaf = path;
    const char *p = path;
    while (*p) {
        if (*p == '/') {
            if (*(p+1) != '\0')
                leaf = p + 1;
        }
        p++;
    }
    return (*leaf) ? leaf : NULL;
}

int vfs_rename(const char *oldpath, const char *newpath)
{
    if (!oldpath || !newpath) return -1;

    vnode_t *v = vfs_lookup(oldpath);
    if (!v) return -1;

    const char *leaf = path_leaf(newpath);
    if (!leaf) return -1;

    size_t nlen = strlen(leaf);
    if (nlen > VNODE_NAME_MAX) return -1;

    memcpy(v->name, leaf, nlen);
    v->name[nlen] = '\0';
    return 0;
}

/* ── Process StdIO Helper ────────────────────────────────────────────────── */
#include "kernel/proc/process.h"

void proc_init_stdio(process_t *proc)
{
    vnode_t *con = vfs_lookup("/zirv/tty/virtual0");
    if (!con) return;

    for (int i = 0; i < 3; i++) {
        open_file_t *f = (open_file_t *)kmalloc(sizeof(open_file_t), 0);
        if (!f) continue;
        memset(f, 0, sizeof(open_file_t));
        f->type  = FILE_TYPE_VFS;
        f->vnode = con;
        proc->fds[i] = f;
    }
}
