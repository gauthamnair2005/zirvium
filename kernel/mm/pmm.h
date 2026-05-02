/* kernel/mm/pmm.h
 * Zirvium Kernel — Physical Memory Manager
 *
 * Uses a bitmap allocator over a fixed pool.  The Multiboot2 memory-map
 * tag is parsed once during boot to discover usable RAM regions; all
 * other regions (firmware reserved, ACPI, MMIO …) are marked unavailable.
 */
#ifndef ZIRVIUM_KERNEL_MM_PMM_H
#define ZIRVIUM_KERNEL_MM_PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE   4096UL
#define PAGE_SHIFT  12

/* Convert between physical addresses and page frame numbers */
#define ADDR_TO_PFN(a)  ((a) >> PAGE_SHIFT)
#define PFN_TO_ADDR(p)  ((uint64_t)(p) << PAGE_SHIFT)

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * pmm_init - initialise the physical memory manager.
 * @mmap_addr: physical address of the Multiboot2 mmap tag entries array
 * @mmap_len:  byte length of the entries array
 * @entry_size: size of one mmap entry reported by Multiboot2
 *
 * Must be called before any pmm_alloc/pmm_free call.
 */
void pmm_init(uint64_t mmap_addr, uint32_t mmap_len, uint32_t entry_size);

/**
 * pmm_alloc_page - allocate one physical page (4 KiB).
 * Returns the physical address of the allocated page, or 0 on failure.
 */
uint64_t pmm_alloc_page(void);

/**
 * pmm_free_page - release a previously allocated physical page.
 * @phys: physical address returned by pmm_alloc_page().
 */
void pmm_free_page(uint64_t phys);

/**
 * pmm_alloc_pages - allocate @count contiguous physical pages.
 * Returns the physical address of the first page, or 0 on failure.
 */
uint64_t pmm_alloc_pages(size_t count);

/**
 * pmm_free_pages - release @count contiguous pages starting at @phys.
 */
void pmm_free_pages(uint64_t phys, size_t count);

/** pmm_total_pages - return total pages tracked by the PMM. */
size_t pmm_total_pages(void);

/** pmm_free_page_count - return number of currently free pages. */
size_t pmm_free_page_count(void);

#endif /* ZIRVIUM_KERNEL_MM_PMM_H */
