#include <kernel/memory.h>
#include <kernel/kernel.h>

#define BITMAP_SIZE 32768
#define TOTAL_PAGES (BITMAP_SIZE * 8)

static uint8_t bitmap[BITMAP_SIZE];
static uint64_t free_pages = TOTAL_PAGES;

static void bitmap_set(uint64_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

static void bitmap_clear(uint64_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static int bitmap_test(uint64_t bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

void pmm_init(void) {
    for (uint64_t i = 0; i < BITMAP_SIZE; i++) {
        bitmap[i] = 0;
    }
    
    for (uint64_t i = 0; i < 256; i++) {
        bitmap_set(i);
        free_pages--;
    }
}

void *pmm_alloc(void) {
    for (uint64_t i = 0; i < TOTAL_PAGES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            return (void*)(i * PAGE_SIZE);
        }
    }
    return NULL;
}

void pmm_free(void *addr) {
    uint64_t page = (uint64_t)addr / PAGE_SIZE;
    if (page < TOTAL_PAGES && bitmap_test(page)) {
        bitmap_clear(page);
        free_pages++;
    }
}
