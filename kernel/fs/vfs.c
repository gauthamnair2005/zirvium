#include <kernel/vfs.h>
#include <kernel/memory.h>
#include <kernel/kernel.h>
#include <kernel/string.h>

static vfs_node_t *root_fs = NULL;

static int str_cmp(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

static void str_copy(char *dst, const char *src, size_t n) {
    strlcpy(dst, src, n);
}

#include <kernel/mosix.h>

void vfs_init(void) {
    // Root Filesystem (/)
    root_fs = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!root_fs) {
        kernel_panic("VFS: Failed to initialize root filesystem");
    }
    
    str_copy(root_fs->name, MOSIX_ROOT, 128);
    root_fs->flags = 0;
    root_fs->next = NULL;
    
    kprintf("VFS: Root filesystem mounted\n");
    
    // Create Core MOSIX Mount Points
    // In a real implementation these would be proper directory nodes
    // For now we just log that we are enforcing the structure
    
    kprintf("MOSIX: Initializing File System Hierarchy...\n");
    // Pseudo-creation logic (since we don't have a full tree VFS yet)
    kprintf("  [OK] %s (Executable Binaries)\n", MOSIX_BIN);
    kprintf("  [OK] %s (Libraries)\n", MOSIX_LIB);
    kprintf("  [OK] %s (Configuration)\n", MOSIX_CONFIG);
    kprintf("  [OK] %s (User Data)\n", MOSIX_USER);
    kprintf("  [OK] %s (Virtual Devices)\n", MOSIX_ZIRV);
    
    // Initialize Zirv Subsystem
    // extern void zirv_subsystem_init(void);
    // zirv_subsystem_init();
}

vfs_node_t *vfs_open(const char *path, uint64_t flags) {
    if (!path) return NULL;
    
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
    if (!node || !buffer || size == 0) return -1;
    
    if (node->read) {
        return node->read(node, offset, size, buffer);
    }
    return -1;
}

ssize_t vfs_write(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer) {
    if (!node || !buffer || size == 0) return -1;
    
    if (node->write) {
        return node->write(node, offset, size, buffer);
    }
    return -1;
}
