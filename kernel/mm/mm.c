#include <kernel/memory.h>
#include <kernel/kernel.h>

extern void pmm_init(void);
extern void vmm_init(void);
extern void kmalloc_init(void);

static uint64_t total_memory = 0;
static uint64_t used_memory = 0;

void mm_init(void) {
    // Initialize physical memory manager
    pmm_init();
    
    // Initialize virtual memory manager
    vmm_init();
    
    // Initialize kernel heap allocator
    kmalloc_init();
    
    // Assume 128MB available for this demo
    total_memory = 128 * 1024 * 1024;
    used_memory = 16 * 1024 * 1024;  // Kernel + reserved
}

uint64_t mm_get_total_memory(void) {
    return total_memory;
}

uint64_t mm_get_used_memory(void) {
    return used_memory;
}

uint64_t mm_get_free_memory(void) {
    return total_memory - used_memory;
}
