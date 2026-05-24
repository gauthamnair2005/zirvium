/* kernel/gpu/gem.h
 * Zirvium Kernel — Graphics Execution Manager (GEM)
 */
#ifndef ZIRVIUM_KERNEL_GPU_GEM_H
#define ZIRVIUM_KERNEL_GPU_GEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define GEM_MAX_BUFFERS 1024

typedef struct gem_buffer {
    uint32_t handle;
    size_t   size;
    uint64_t phys_addr;
    void    *virt_addr;
    uint32_t ref_count;
    bool     mapped;
} gem_buffer_t;

/**
 * gem_init - Initialize the GEM subsystem.
 */
void gem_init(void);

/**
 * gem_alloc - Allocate a new GPU buffer object.
 */
uint32_t gem_alloc(size_t size);

/**
 * gem_free - Free a GPU buffer object by handle.
 */
void gem_free(uint32_t handle);

/**
 * gem_get - Get a buffer object by handle (increments refcount).
 */
gem_buffer_t *gem_get(uint32_t handle);

/**
 * gem_put - Release a buffer object reference.
 */
void gem_put(gem_buffer_t *bo);

#endif /* ZIRVIUM_KERNEL_GPU_GEM_H */
