#include <kernel/memory.h>
#include <kernel/kernel.h>

extern void *pmm_alloc(void);

/* 32-bit paging structures */
#define PD_INDEX(addr)   (((addr) >> 22) & 0x3FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x3FF)

static uint32_t *kernel_page_dir = NULL;

static uint32_t *get_or_create_table(uint32_t *table, uint32_t index) {
    if (!(table[index] & PAGE_PRESENT)) {
        uint32_t *new_table = (uint32_t*)pmm_alloc();
        if (!new_table) return NULL;
        
        for (int i = 0; i < 1024; i++) {
            new_table[i] = 0;
        }
        
        table[index] = (uint32_t)new_table | PAGE_PRESENT | PAGE_WRITE;
    }
    return (uint32_t*)(table[index] & ~0xFFF);
}

void vmm_init(void) {
    // For now, just initialize structures without enabling paging
    // We're already in a flat memory model which works fine
    kernel_page_dir = (uint32_t*)pmm_alloc();
    
    if (!kernel_page_dir) {
        return;
    }
    
    for (int i = 0; i < 1024; i++) {
        kernel_page_dir[i] = 0;
    }
    
    /* Identity map first 4MB for kernel */
    for (uint32_t addr = 0; addr < 0x400000; addr += PAGE_SIZE) {
        vmm_map(addr, addr, PAGE_PRESENT | PAGE_WRITE);
    }
    
    // DON'T enable paging yet - we're fine with flat memory model
    // asm volatile("mov %0, %%cr3" :: "r"(kernel_page_dir));
}

void vmm_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    if (!kernel_page_dir) return;
    
    uint32_t *pt = get_or_create_table(kernel_page_dir, PD_INDEX(virt));
    if (!pt) return;
    
    pt[PT_INDEX(virt)] = phys | flags;
}

void *vmm_alloc(size_t pages) {
    static uint32_t next_virt = 0x10000000;  /* Start at 256MB */
    
    uint32_t virt = next_virt;
    for (size_t i = 0; i < pages; i++) {
        uint32_t phys = (uint32_t)pmm_alloc();
        if (!phys) return NULL;
        
        vmm_map(next_virt, phys, PAGE_PRESENT | PAGE_WRITE);
        next_virt += PAGE_SIZE;
    }
    
    return (void*)(uintptr_t)virt;
}

void vmm_free(void *addr, size_t pages) {
    (void)addr;
    (void)pages;
    /* TODO: Implement page unmapping */
}
