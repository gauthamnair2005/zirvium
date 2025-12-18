#ifndef KERNEL_VFS_H
#define KERNEL_VFS_H

#include <kernel/types.h>

#define MAX_PATH 256
#define MAX_FILES 256

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0100

typedef struct vfs_node {
    char name[128];
    uint64_t inode;
    uint64_t size;
    uint64_t flags;
    
    ssize_t (*read)(struct vfs_node *, uint64_t, uint64_t, uint8_t *);
    ssize_t (*write)(struct vfs_node *, uint64_t, uint64_t, uint8_t *);
    void (*open)(struct vfs_node *);
    void (*close)(struct vfs_node *);
    
    struct vfs_node *next;
} vfs_node_t;

void vfs_init(void);
vfs_node_t *vfs_open(const char *path, uint64_t flags);
void vfs_close(vfs_node_t *node);
ssize_t vfs_read(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer);
ssize_t vfs_write(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer);

#endif
