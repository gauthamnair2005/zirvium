/* kernel/mm/pmm.c
 * Zirvium Kernel — Physical Memory Manager implementation
 *
 * Bitmap allocator: one bit per 4-KiB page frame.
 * Bit == 0 → frame is free; bit == 1 → frame is used/reserved.
 *
 * The bitmap itself is placed in the .bss section (statically allocated).
 * Maximum supported physical memory: MAX_PMM_PAGES × PAGE_SIZE = 16 GiB.
 */
#include "pmm.h"
#include <stdint.h>
#include <stddef.h>

/* ── Configuration ────────────────────────────────────────────────────────── */
/* 1 TiB / 4 KiB = 268 435 456 frames → bitmap = 32 MiB */
#define MAX_PMM_PAGES   (1024UL * 1024UL * 1024UL * 1024UL / PAGE_SIZE)

/* ── Multiboot2 memory-map entry type values ─────────────────────────────── */
#define MB2_MMAP_AVAILABLE  1

/* ── Multiboot2 mmap entry layout ─────────────────────────────────────────── */
typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

/* ── Bitmap storage ──────────────────────────────────────────────────────── */
#define BITMAP_WORDS   (MAX_PMM_PAGES / 64)
static uint64_t bitmap[BITMAP_WORDS];   /* 0 = free, 1 = used */

static size_t total_pages  = 0;
static size_t free_pages   = 0;
static size_t search_start = 0;   /* hint for next free page search */

/* ── Bitmap helpers ──────────────────────────────────────────────────────── */
static inline void bitmap_set(size_t pfn) {
    bitmap[pfn / 64] |= (1ULL << (pfn % 64));
}
static inline void bitmap_clear(size_t pfn) {
    bitmap[pfn / 64] &= ~(1ULL << (pfn % 64));
}
static inline int bitmap_test(size_t pfn) {
    return (bitmap[pfn / 64] >> (pfn % 64)) & 1;
}

/* Mark all pages in [base, base+len) as free */
static void mark_free_region(uint64_t base, uint64_t len)
{
    uint64_t start_pfn = (base + PAGE_SIZE - 1) >> PAGE_SHIFT;   /* round up */
    uint64_t end_pfn   = (base + len) >> PAGE_SHIFT;             /* round down */

    for (uint64_t pfn = start_pfn; pfn < end_pfn && pfn < MAX_PMM_PAGES; pfn++) {
        if (bitmap_test((size_t)pfn)) {
            bitmap_clear((size_t)pfn);
            free_pages++;
        }
    }
    if (end_pfn > total_pages)
        total_pages = (size_t)end_pfn;
}

/* ── Public implementation ──────────────────────────────────────────────── */
void pmm_init(uint64_t mmap_addr, uint32_t mmap_len, uint32_t entry_size)
{
    /* Start with everything marked used */
    for (size_t i = 0; i < BITMAP_WORDS; i++)
        bitmap[i] = ~0ULL;
    free_pages  = 0;
    total_pages = 0;

    /* Walk the Multiboot2 memory map */
    uint64_t off = 0;
    while (off + entry_size <= mmap_len) {
        const mb2_mmap_entry_t *e =
            (const mb2_mmap_entry_t *)(uintptr_t)(mmap_addr + off);
        if (e->type == MB2_MMAP_AVAILABLE && e->length > 0)
            mark_free_region(e->base_addr, e->length);
        off += entry_size;
    }

    /* Reserve page 0 (real-mode IVT / BIOS data area) */
    if (total_pages > 0 && !bitmap_test(0)) {
        bitmap_set(0);
        if (free_pages > 0) free_pages--;
    }

    search_start = 1;
}

uint64_t pmm_alloc_page(void)
{
    for (size_t i = search_start; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            search_start = i + 1;
            return PFN_TO_ADDR(i);
        }
    }
    /* Wrap around from the beginning */
    for (size_t i = 1; i < search_start; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            search_start = i + 1;
            return PFN_TO_ADDR(i);
        }
    }
    return 0;   /* OOM */
}

void pmm_free_page(uint64_t phys)
{
    size_t pfn = (size_t)ADDR_TO_PFN(phys);
    if (pfn < total_pages && bitmap_test(pfn)) {
        bitmap_clear(pfn);
        free_pages++;
        if (pfn < search_start)
            search_start = pfn;
    }
}

uint64_t pmm_alloc_pages(size_t count)
{
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();

    /* Scan for a contiguous run of `count` free pages */
    size_t run = 0;
    for (size_t i = 1; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            run++;
            if (run == count) {
                size_t start = i - count + 1;
                for (size_t j = start; j <= i; j++) {
                    bitmap_set(j);
                    free_pages--;
                }
                return PFN_TO_ADDR(start);
            }
        } else {
            run = 0;
        }
    }
    return 0;   /* not enough contiguous pages */
}

void pmm_free_pages(uint64_t phys, size_t count)
{
    for (size_t i = 0; i < count; i++)
        pmm_free_page(phys + (uint64_t)i * PAGE_SIZE);
}

size_t pmm_total_pages(void)     { return total_pages; }
size_t pmm_free_page_count(void) { return free_pages;  }
