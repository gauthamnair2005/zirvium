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
    
    // Enable paging
    asm volatile("mov %0, %%cr3" :: "r"(kernel_page_dir));
    
    // Enable paging bit in CR0
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

void vmm_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    if (!kernel_page_dir) return;
    
    uint32_t pd_idx = PD_INDEX(virt);
    uint32_t pt_idx = PT_INDEX(virt);
    
    if (pd_idx >= 1024 || pt_idx >= 1024) return;
    
    uint32_t *pt = get_or_create_table(kernel_page_dir, pd_idx);
    if (!pt) return;
    
    pt[pt_idx] = phys | flags;
    
    /* Flush TLB for this address */
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
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
    if (!addr || !kernel_page_dir) return;
    
    uint32_t virt = (uint32_t)(uintptr_t)addr;
    
    for (size_t i = 0; i < pages; i++) {
        uint32_t pd_idx = PD_INDEX(virt);
        uint32_t pt_idx = PT_INDEX(virt);
        
        if (pd_idx >= 1024 || !(kernel_page_dir[pd_idx] & PAGE_PRESENT)) {
            virt += PAGE_SIZE;
            continue;
        }
        
        uint32_t *pt = (uint32_t*)(kernel_page_dir[pd_idx] & ~0xFFF);
        if (pt[pt_idx] & PAGE_PRESENT) {
            uint32_t phys = pt[pt_idx] & ~0xFFF;
            pmm_free((void*)(uintptr_t)phys);
            pt[pt_idx] = 0;
            asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
        }
        
        virt += PAGE_SIZE;
    }
}
