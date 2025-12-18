#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include <kernel/types.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define PHYS_BASE 0xFFFF800000000000UL
#define KERNEL_BASE 0xFFFFFFFF80000000UL

#define PAGE_PRESENT (1UL << 0)
#define PAGE_WRITE   (1UL << 1)
#define PAGE_USER    (1UL << 2)

void mm_init(void);
void *pmm_alloc(void);
void pmm_free(void *addr);

void *vmm_alloc(size_t pages);
void vmm_free(void *addr, size_t pages);
void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);

void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
