#include <kernel/vfs.h>
#include <kernel/memory.h>
#include <kernel/kernel.h>

static vfs_node_t *root_fs = NULL;

static int str_cmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

static void str_copy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

void vfs_init(void) {
    root_fs = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!root_fs) {
        kernel_panic("VFS: Failed to initialize root filesystem");
    }
    
    str_copy(root_fs->name, "/", 128);
    root_fs->inode = 0;
    root_fs->size = 0;
    root_fs->flags = 0;
    root_fs->read = NULL;
    root_fs->write = NULL;
    root_fs->open = NULL;
    root_fs->close = NULL;
    root_fs->next = NULL;
    
    kprintf("VFS: Root filesystem mounted\n");
}

vfs_node_t *vfs_open(const char *path, uint64_t flags) {
    (void)flags;
    vfs_node_t *node = root_fs;
    
    while (node) {
        if (str_cmp(node->name, path) == 0) {
            if (node->open) {
                node->open(node);
            }
            return node;
        }
        node = node->next;
    }
    
    return NULL;
}

void vfs_close(vfs_node_t *node) {
    if (node && node->close) {
        node->close(node);
    }
}

ssize_t vfs_read(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer) {
    if (node && node->read) {
        return node->read(node, offset, size, buffer);
    }
    return -1;
}

ssize_t vfs_write(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer) {
    if (node && node->write) {
        return node->write(node, offset, size, buffer);
    }
    return -1;
}
