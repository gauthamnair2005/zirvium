/* kernel/mm/heap.c
 * Zirvium Kernel — kernel heap (bump / slab allocator)
 *
 * Provides kzalloc / kmalloc / kfree used by the Linux compat layer.
 * Phase 1: simple bump allocator over a static arena.
 * Phase 2 (TODO): replace with a proper slab + page-based allocator.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define HEAP_SIZE (4 * 1024 * 1024)   /* 4 MiB static heap */

static uint8_t heap_arena[HEAP_SIZE];
static size_t  heap_offset = 0;

/* ── Block header for kfree (minimal free-list) ────────────────────────────
 * Layout:  [magic(4)] [size(8)] [data …]
 * free() currently marks the header as freed but does not coalesce.
 */
#define HEAP_MAGIC_ALLOC  0xA110CA7EU
#define HEAP_MAGIC_FREE   0xFEEDF00DU

typedef struct {
    uint32_t magic;
    size_t   size;   /* size of data region (excludes header) */
} heap_hdr_t;

#define HDR_SIZE  ALIGN_UP(sizeof(heap_hdr_t), 16)
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

void *kmalloc(size_t size, unsigned int flags)
{
    (void)flags;
    if (size == 0) return NULL;
    size = ALIGN_UP(size, 16);
    size_t need = HDR_SIZE + size;
    if (heap_offset + need > HEAP_SIZE) return NULL;

    heap_hdr_t *hdr = (heap_hdr_t *)(heap_arena + heap_offset);
    hdr->magic = HEAP_MAGIC_ALLOC;
    hdr->size  = size;
    heap_offset += need;
    return (uint8_t *)hdr + HDR_SIZE;
}

void *kzalloc(size_t size, unsigned int flags)
{
    void *p = kmalloc(size, flags);
    if (p) memset(p, 0, size);
    return p;
}

void *kcalloc(size_t n, size_t size, unsigned int flags)
{
    return kzalloc(n * size, flags);
}

void kfree(void *ptr);

void *krealloc(void *ptr, size_t new_size, unsigned int flags)
{
    if (!ptr) return kmalloc(new_size, flags);
    heap_hdr_t *hdr = (heap_hdr_t *)((uint8_t *)ptr - HDR_SIZE);
    if (hdr->magic != HEAP_MAGIC_ALLOC) return NULL;
    if (new_size <= hdr->size) return ptr;   /* fits in existing block */
    void *np = kmalloc(new_size, flags);
    if (!np) return NULL;
    memcpy(np, ptr, hdr->size);
    kfree(ptr);
    return np;
}

void kfree(void *ptr)
{
    if (!ptr) return;
    heap_hdr_t *hdr = (heap_hdr_t *)((uint8_t *)ptr - HDR_SIZE);
    if (hdr->magic != HEAP_MAGIC_ALLOC) return;
    hdr->magic = HEAP_MAGIC_FREE;
    /* TODO: add to free list and coalesce */
}
