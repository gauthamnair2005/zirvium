/* kernel/gpu/gem.c
 * Zirvium Kernel — Graphics Execution Manager (GEM) implementation
 */
#include "gem.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/console.h"
#include <string.h>

extern void *kmalloc(size_t size, unsigned int flags);
extern void  kfree(void *ptr);

static gem_buffer_t *g_buffers[GEM_MAX_BUFFERS];
static uint32_t      g_next_handle = 1;

void gem_init(void)
{
    memset(g_buffers, 0, sizeof(g_buffers));
    klog(LOG_OK, "gem", "GEM subsystem initialised");
}

uint32_t gem_alloc(size_t size)
{
    if (size == 0) return 0;

    /* Align size to page size */
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t pages = size / PAGE_SIZE;

    uint64_t phys = pmm_alloc_pages(pages);
    if (!phys) return 0;

    gem_buffer_t *bo = (gem_buffer_t *)kmalloc(sizeof(gem_buffer_t), 0);
    if (!bo) {
        pmm_free_pages(phys, pages);
        return 0;
    }

    bo->handle    = g_next_handle++;
    bo->size      = size;
    bo->phys_addr = phys;
    bo->virt_addr = PHYS_TO_VIRT(phys);
    bo->ref_count = 1;
    bo->mapped    = false;

    /* Find a slot */
    for (int i = 0; i < GEM_MAX_BUFFERS; i++) {
        if (!g_buffers[i]) {
            g_buffers[i] = bo;
            return bo->handle;
        }
    }

    /* Table full */
    pmm_free_pages(phys, pages);
    kfree(bo);
    return 0;
}

gem_buffer_t *gem_get(uint32_t handle)
{
    if (handle == 0) return NULL;
    for (int i = 0; i < GEM_MAX_BUFFERS; i++) {
        if (g_buffers[i] && g_buffers[i]->handle == handle) {
            g_buffers[i]->ref_count++;
            return g_buffers[i];
        }
    }
    return NULL;
}

void gem_put(gem_buffer_t *bo)
{
    if (!bo) return;
    if (--bo->ref_count == 0) {
        /* Remove from table */
        for (int i = 0; i < GEM_MAX_BUFFERS; i++) {
            if (g_buffers[i] == bo) {
                g_buffers[i] = NULL;
                break;
            }
        }
        pmm_free_pages(bo->phys_addr, bo->size / PAGE_SIZE);
        kfree(bo);
    }
}

void gem_free(uint32_t handle)
{
    gem_buffer_t *bo = gem_get(handle);
    if (bo) {
        gem_put(bo); /* release gem_get ref */
        gem_put(bo); /* release original alloc ref */
    }
}
