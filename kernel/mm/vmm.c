#include <kernel/memory.h>
#include <kernel/kernel.h>

extern void *pmm_alloc(void);

#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)

static uint64_t *kernel_pml4 = NULL;

static uint64_t *get_or_create_table(uint64_t *table, uint64_t index) {
    if (!(table[index] & PAGE_PRESENT)) {
        uint64_t *new_table = pmm_alloc();
        if (!new_table) return NULL;
        
        for (int i = 0; i < 512; i++) {
            ((uint64_t*)new_table)[i] = 0;
        }
        
        table[index] = (uint64_t)new_table | PAGE_PRESENT | PAGE_WRITE;
    }
    return (uint64_t*)(table[index] & ~0xFFF);
}

void vmm_init(void) {
    kernel_pml4 = pmm_alloc();
    
    for (int i = 0; i < 512; i++) {
        kernel_pml4[i] = 0;
    }
    
    for (uint64_t addr = 0; addr < 0x100000000; addr += PAGE_SIZE) {
        vmm_map(PHYS_BASE + addr, addr, PAGE_PRESENT | PAGE_WRITE);
    }
    
    asm volatile("mov %0, %%cr3" :: "r"(kernel_pml4));
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!kernel_pml4) return;
    
    uint64_t *pdpt = get_or_create_table(kernel_pml4, PML4_INDEX(virt));
    if (!pdpt) return;
    
    uint64_t *pd = get_or_create_table(pdpt, PDPT_INDEX(virt));
    if (!pd) return;
    
    uint64_t *pt = get_or_create_table(pd, PD_INDEX(virt));
    if (!pt) return;
    
    pt[PT_INDEX(virt)] = phys | flags;
}

void *vmm_alloc(size_t pages) {
    static uint64_t next_virt = KERNEL_BASE + 0x10000000;
    
    uint64_t virt = next_virt;
    for (size_t i = 0; i < pages; i++) {
        uint64_t phys = (uint64_t)pmm_alloc();
        if (!phys) return NULL;
        
        vmm_map(next_virt, phys, PAGE_PRESENT | PAGE_WRITE);
        next_virt += PAGE_SIZE;
    }
    
    return (void*)virt;
}

void vmm_free(void *addr, size_t pages) {
    uint64_t virt = (uint64_t)addr;
    for (size_t i = 0; i < pages; i++) {
        virt += PAGE_SIZE;
    }
}
