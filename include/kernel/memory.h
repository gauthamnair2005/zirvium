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

/* Memory Manager */
void mm_init(void);
uint64_t mm_get_total_memory(void);
uint64_t mm_get_used_memory(void);
uint64_t mm_get_free_memory(void);

/* Physical Memory Manager */
void pmm_init(void);
void *pmm_alloc(void);
void pmm_free(void *addr);
uint32_t pmm_get_free_pages(void);
uint32_t pmm_get_total_pages(void);

/* Virtual Memory Manager */
void vmm_init(void);
void *vmm_alloc(size_t pages);
void vmm_free(void *addr, size_t pages);
void vmm_map(uint32_t virt, uint32_t phys, uint32_t flags);

/* Kernel Heap Allocator */
void kmalloc_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t kmalloc_get_used(void);
size_t kmalloc_get_available(void);

#endif
