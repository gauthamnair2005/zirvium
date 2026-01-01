#include <kernel/memory.h>
#include <kernel/kernel.h>

#define BITMAP_SIZE 32768
#define TOTAL_PAGES (BITMAP_SIZE * 8)

static uint8_t bitmap[BITMAP_SIZE];
static uint32_t free_pages = TOTAL_PAGES;

static void bitmap_set(uint32_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

static void bitmap_clear(uint32_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static int bitmap_test(uint32_t bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

void pmm_init(void) {
    // Clear bitmap - all pages initially free
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) {
        bitmap[i] = 0;
    }
    
    // Reserve first 1MB (256 pages) for kernel and BIOS
    for (uint32_t i = 0; i < 256; i++) {
        bitmap_set(i);
        free_pages--;
    }
}

uint32_t pmm_get_free_pages(void) {
    return free_pages;
}

uint32_t pmm_get_total_pages(void) {
    return TOTAL_PAGES;
}

void *pmm_alloc(void) {
    for (uint32_t i = 0; i < TOTAL_PAGES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            return (void*)(uintptr_t)(i * PAGE_SIZE);
        }
    }
    return NULL;
}

void pmm_free(void *addr) {
    if (!addr) return;
    
    uint32_t page = (uint32_t)(uintptr_t)addr / PAGE_SIZE;
    if (page < TOTAL_PAGES && bitmap_test(page)) {
        bitmap_clear(page);
        if (free_pages < TOTAL_PAGES) {
            free_pages++;
        }
    }
}
