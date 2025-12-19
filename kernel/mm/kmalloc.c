#include <kernel/memory.h>
#include <kernel/kernel.h>

#define HEAP_SIZE (4 * 1024 * 1024)  // 4MB kernel heap

static uint8_t heap[HEAP_SIZE];
static size_t heap_pos = 0;
static size_t allocations = 0;
static size_t deallocations = 0;

typedef struct block {
    size_t size;
    int free;
    struct block *next;
} block_t;

static block_t *free_list = NULL;
static int heap_initialized = 0;

void kmalloc_init(void) {
    heap_pos = 0;
    free_list = NULL;
    allocations = 0;
    deallocations = 0;
    heap_initialized = 1;
}

void *kmalloc(size_t size) {
    if (!heap_initialized || size == 0) return NULL;
    
    // Align to 16-byte boundary for performance
    size = (size + 15) & ~15;
    
    // Try to find a free block first
    block_t *curr = free_list;
    block_t *prev = NULL;
    
    while (curr) {
        if (curr->free && curr->size >= size) {
            curr->free = 0;
            allocations++;
            return (void*)((uint64_t)curr + sizeof(block_t));
        }
        prev = curr;
        curr = curr->next;
    }
    
    // Check if we have space
    if (heap_pos + sizeof(block_t) + size > HEAP_SIZE) {
        kprintf("kmalloc: Out of memory (requested: %lu bytes)\n", size);
        return NULL;
    }
    
    // Allocate new block
    block_t *block = (block_t*)&heap[heap_pos];
    block->size = size;
    block->free = 0;
    block->next = NULL;
    
    if (prev) {
        prev->next = block;
    } else {
        free_list = block;
    }
    
    heap_pos += sizeof(block_t) + size;
    allocations++;
    
    return (void*)((uint64_t)block + sizeof(block_t));
}

void kfree(void *ptr) {
    if (!ptr || !heap_initialized) return;
    
    block_t *block = (block_t*)((uint64_t)ptr - sizeof(block_t));
    block->free = 1;
    deallocations++;
}

size_t kmalloc_get_used(void) {
    return heap_pos;
}

size_t kmalloc_get_available(void) {
    return HEAP_SIZE - heap_pos;
}
