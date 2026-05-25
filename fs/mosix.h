/* fs/mosix.h
 * Zirvium Kernel — MOSIX (Modern OSIX) Virtual File System
 *
 * MOSIX defines a simplified filesystem hierarchy that replaces the
 * POSIX/FHS layout with a cleaner, purpose-driven structure:
 *
 *  /bin      — executable binaries only (execute permission, no libraries)
 *  /lib      — non-executable library (.a/.so) and header files
 *  /user     — user home directories (replaces /home; NOT /usr)
 *  /boot     — bootloader and kernel images (same role as POSIX /boot)
 *  /config   — machine-readable & human-readable configuration (replaces /etc)
 *  /zirv     — virtual device namespace (replaces /dev, /run, /sys)
 *  /mounts   — mount points for removable/additional filesystems (replaces /mnt)
 *  /tmp      — ephemeral temporary files (same role as POSIX /tmp)
 *
 * Device nodes under /zirv follow a technology/type path scheme:
 *  /zirv/sata/{hdd,ssd,cdrom,cdrw,dvdrom,dvdrw}   — SATA-attached devices
 *  /zirv/pata/{hdd,ssd,cdrom,cdrw,dvdrom,dvdrw}   — PATA/IDE-attached devices
 *  /zirv/nvme/ssd                                  — NVMe PCIe SSDs
 *  /zirv/usb/{pendrive,hdd,ssd,cdrom}              — USB storage
 *  /zirv/net/{eth,wlan,lo}                         — network interfaces
 *  /zirv/input/{keyboard,mouse,touchpad}           — input devices
 *  /zirv/display/{framebuffer,gpu}                 — display devices
 *  /zirv/tty/{serial,virtual}                      — terminals
 *  /zirv/audio/{output,input}                      — audio devices
 */
#ifndef ZIRVIUM_FS_MOSIX_H
#define ZIRVIUM_FS_MOSIX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Forward declarations ─────────────────────────────────────────────────── */
struct vnode;
struct vfs;
struct file;

/* ── VNode types ──────────────────────────────────────────────────────────── */
typedef enum {
    VNODE_FILE      = 0,
    VNODE_DIR       = 1,
    VNODE_DEVICE    = 2,   /* leaf node in /zirv; represents a physical device */
    VNODE_SYMLINK   = 3,
    VNODE_MOUNTPT   = 4,
} vnode_type_t;

/* ── MOSIX permission bits (simplified, no group/world suid) ─────────────── */
#define MOSIX_PERM_READ     0x04
#define MOSIX_PERM_WRITE    0x02
#define MOSIX_PERM_EXEC     0x01

/* ── Device class identifiers ─────────────────────────────────────────────── */
typedef enum {
    DEV_CLASS_UNKNOWN = 0,

    /* Storage buses */
    DEV_CLASS_SATA,
    DEV_CLASS_PATA,
    DEV_CLASS_NVME,
    DEV_CLASS_USB,

    /* Storage media types */
    DEV_CLASS_HDD,
    DEV_CLASS_SSD,
    DEV_CLASS_CDROM,
    DEV_CLASS_CDRW,
    DEV_CLASS_DVDROM,
    DEV_CLASS_DVDRW,
    DEV_CLASS_PENDRIVE,

    /* Network */
    DEV_CLASS_NET_ETH,
    DEV_CLASS_NET_WLAN,
    DEV_CLASS_NET_BT,
    DEV_CLASS_NET_LOOPBACK,

    /* Input */
    DEV_CLASS_INPUT_KEYBOARD,
    DEV_CLASS_INPUT_MOUSE,
    DEV_CLASS_INPUT_TOUCHPAD,

    /* Display */
    DEV_CLASS_DISPLAY_FB,
    DEV_CLASS_DISPLAY_GPU,

    /* Terminal */
    DEV_CLASS_TTY_SERIAL,
    DEV_CLASS_TTY_VIRTUAL,

    /* Audio */
    DEV_CLASS_AUDIO_OUTPUT,
    DEV_CLASS_AUDIO_INPUT,

    DEV_CLASS_MAX,
} dev_class_t;

/* ── Device descriptor (attached to a VNODE_DEVICE node) ─────────────────── */
typedef struct device_desc {
    uint32_t     major;         /* device major number   */
    uint32_t     minor;         /* device minor number   */
    dev_class_t  bus_class;     /* e.g. DEV_CLASS_SATA   */
    dev_class_t  media_class;   /* e.g. DEV_CLASS_HDD    */
    char         model[64];     /* human-readable model string */
    bool         present;       /* hardware detected?    */
    void        *driver_data;   /* opaque driver-private pointer */
} device_desc_t;

/* ── VNode operations table ───────────────────────────────────────────────── */
typedef struct vnode_ops {
    int  (*read)(struct vnode *v, void *buf, size_t len, uint64_t off);
    int  (*write)(struct vnode *v, const void *buf, size_t len, uint64_t off);
    int  (*readdir)(struct vnode *dir, uint32_t idx, char *name_out, size_t namelen);
    struct vnode *(*lookup)(struct vnode *dir, const char *name);
    int  (*create)(struct vnode *dir, const char *name, vnode_type_t type);
    int  (*unlink)(struct vnode *dir, const char *name);
    void (*release)(struct vnode *v);
} vnode_ops_t;

/* ── VNode ────────────────────────────────────────────────────────────────── */
#define VNODE_NAME_MAX  255

typedef struct vnode {
    char           name[VNODE_NAME_MAX + 1];
    vnode_type_t   type;
    uint8_t        permissions;    /* MOSIX_PERM_* bits */
    uint64_t       size;
    uint64_t       inode;

    /* Tree links */
    struct vnode  *parent;
    struct vnode  *children;       /* first child (if dir) */
    struct vnode  *next_sibling;

    /* File-system ops */
    const vnode_ops_t *ops;

    /* Mount info */
    struct vfs    *mounted_fs;     /* non-NULL if this is a mount point */

    /* Device info (valid when type == VNODE_DEVICE) */
    device_desc_t *device;

    /* Private data for filesystem implementations (tmpfs, etc.) */
    void          *private_data;
} vnode_t;

/* ── VFS (mounted filesystem instance) ───────────────────────────────────── */
typedef struct vfs {
    const char   *type_name;       /* e.g. "zirvfs", "fat32", "ext4" */
    vnode_t      *root;
    void         *fs_data;
} vfs_t;

/* ── MOSIX root namespace ─────────────────────────────────────────────────── */
#define MOSIX_TOP_DIRS 10
extern const char *mosix_top_dirs[MOSIX_TOP_DIRS];

/* ── Public VFS API ───────────────────────────────────────────────────────── */

/**
 * vfs_init - initialise the MOSIX root namespace.
 *
 * Creates the in-memory tree with all standard top-level directories and
 * populates the /zirv sub-namespace with known device class paths.
 */
void vfs_init(void);

/**
 * vfs_lookup - resolve an absolute MOSIX path to a vnode.
 * Returns NULL if the path does not exist.
 */
vnode_t *vfs_lookup(const char *path);

/**
 * vfs_mount - mount a filesystem @fs at @path.
 * The node at @path must already exist and have type VNODE_DIR or VNODE_MOUNTPT.
 */
int vfs_mount(const char *path, vfs_t *fs);

/**
 * vfs_register_device - insert a device node at a canonical /zirv path.
 *
 * @bus_class:   e.g. DEV_CLASS_SATA
 * @media_class: e.g. DEV_CLASS_HDD
 * @index:       instance index (0-based); creates "hdd0", "hdd1", …
 * @desc:        caller-provided device descriptor (kernel takes ownership)
 *
 * Returns the newly created vnode, or NULL on failure.
 */
vnode_t *vfs_register_device(dev_class_t bus_class, dev_class_t media_class,
                              uint32_t index, device_desc_t *desc);

/**
 * vfs_root - return the root vnode of the MOSIX namespace.
 */
vnode_t *vfs_root(void);

/**
 * vfs_mkdir - create a directory at @path (including parent path + leaf name).
 * The parent directory must already exist.
 * Returns 0 on success, -1 on failure.
 */
int vfs_mkdir(const char *path);

/**
 * vfs_rmdir - remove an empty directory at @path.
 * Returns 0 on success, -1 on failure.
 */
int vfs_rmdir(const char *path);

/**
 * vfs_unlink - remove a file at @path.
 * Returns 0 on success, -1 on failure.
 */
int vfs_unlink(const char *path);

/**
 * vfs_rename - rename a file or directory from @oldpath to @newpath.
 * Both paths must reside on the same filesystem (in-memory).
 * Returns 0 on success, -1 on failure.
 */
int vfs_rename(const char *oldpath, const char *newpath);

#endif /* ZIRVIUM_FS_MOSIX_H */
