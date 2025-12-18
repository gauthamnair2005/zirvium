#include <kernel/memory.h>
#include <kernel/kernel.h>

#define HEAP_SIZE (1024 * 1024)

static uint8_t heap[HEAP_SIZE];
static size_t heap_pos = 0;

typedef struct block {
    size_t size;
    int free;
    struct block *next;
} block_t;

static block_t *free_list = NULL;

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    size = (size + 15) & ~15;
    
    block_t *curr = free_list;
    block_t *prev = NULL;
    
    while (curr) {
        if (curr->free && curr->size >= size) {
            curr->free = 0;
            return (void*)((uint64_t)curr + sizeof(block_t));
        }
        prev = curr;
        curr = curr->next;
    }
    
    if (heap_pos + sizeof(block_t) + size > HEAP_SIZE) {
        return NULL;
    }
    
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
    
    return (void*)((uint64_t)block + sizeof(block_t));
}

void kfree(void *ptr) {
    if (!ptr) return;
    
    block_t *block = (block_t*)((uint64_t)ptr - sizeof(block_t));
    block->free = 1;
}
