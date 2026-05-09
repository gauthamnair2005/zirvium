#ifndef ZIRVIUM_KERNEL_LOADER_EMBEDDED_H
#define ZIRVIUM_KERNEL_LOADER_EMBEDDED_H

#include <stddef.h>

typedef struct {
    char  *path;
    void  *data;
    size_t size;
} embedded_binary_t;

extern const embedded_binary_t *embedded_binaries;

/**
 * embedded_init - initialise the embedded binary table with actual
 * data pointers. Must be called once during boot.
 */
void embedded_init(void);

/**
 * embedded_find - look up an embedded binary by path.
 * Returns NULL if not found.
 */
const void *embedded_find(const char *path, size_t *size_out);

#endif