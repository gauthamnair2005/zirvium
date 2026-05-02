/* kernel/mm/vmm.c
 * Zirvium Kernel — Virtual Memory Manager implementation
 */
#include "vmm.h"
#include "pmm.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────────────── */
/* Virtual address → PML4/PDP/PD/PT indices */
#define PML4_IDX(v)  (((v) >> 39) & 0x1FF)
#define PDP_IDX(v)   (((v) >> 30) & 0x1FF)
#define PD_IDX(v)    (((v) >> 21) & 0x1FF)
#define PT_IDX(v)    (((v) >> 12) & 0x1FF)

/* Physical address of a page-table entry's child table */
#define PTE_CHILD_PHYS(e)  ((e) & 0x000FFFFFFFFFF000ULL)

static address_space_t kernel_as;

/* ── Low-level page-table helpers ────────────────────────────────────────── */

/* Get (or allocate) a child table pointer from a parent entry */
static pte_t *get_or_alloc_table(pte_t *parent_entry, uint64_t flags)
{
    if (!(*parent_entry & PTE_PRESENT)) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return NULL;
        memset(PHYS_TO_VIRT(phys), 0, PAGE_SIZE);
        *parent_entry = phys | flags | PTE_PRESENT;
    }
    return (pte_t *)PHYS_TO_VIRT(PTE_CHILD_PHYS(*parent_entry));
}

/* ── Public implementation ──────────────────────────────────────────────── */

void vmm_map_page(address_space_t *as, uint64_t virt, uint64_t phys,
                  uint64_t flags)
{
    address_space_t *target = as ? as : &kernel_as;
    pte_t *pml4 = (pte_t *)PHYS_TO_VIRT(target->pml4_phys);

    uint64_t table_flags = PTE_PRESENT | PTE_WRITABLE | PTE_USER;

    pte_t *pdp = get_or_alloc_table(&pml4[PML4_IDX(virt)], table_flags);
    if (!pdp) return;
    pte_t *pd  = get_or_alloc_table(&pdp[PDP_IDX(virt)],  table_flags);
    if (!pd)  return;
    pte_t *pt  = get_or_alloc_table(&pd[PD_IDX(virt)],    table_flags);
    if (!pt)  return;

    pt[PT_IDX(virt)] = (phys & 0x000FFFFFFFFFF000ULL) | flags | PTE_PRESENT;

    /* Invalidate the TLB entry */
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_unmap_page(address_space_t *as, uint64_t virt)
{
    address_space_t *target = as ? as : &kernel_as;
    pte_t *pml4 = (pte_t *)PHYS_TO_VIRT(target->pml4_phys);

    pte_t *pdp_e = &pml4[PML4_IDX(virt)];
    if (!(*pdp_e & PTE_PRESENT)) return;
    pte_t *pdp = (pte_t *)PHYS_TO_VIRT(PTE_CHILD_PHYS(*pdp_e));

    pte_t *pd_e = &pdp[PDP_IDX(virt)];
    if (!(*pd_e & PTE_PRESENT)) return;
    pte_t *pd = (pte_t *)PHYS_TO_VIRT(PTE_CHILD_PHYS(*pd_e));

    pte_t *pt_e = &pd[PD_IDX(virt)];
    if (!(*pt_e & PTE_PRESENT)) return;
    pte_t *pt = (pte_t *)PHYS_TO_VIRT(PTE_CHILD_PHYS(*pt_e));

    pt[PT_IDX(virt)] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t vmm_virt_to_phys(address_space_t *as, uint64_t virt)
{
    address_space_t *target = as ? as : &kernel_as;
    pte_t *pml4 = (pte_t *)PHYS_TO_VIRT(target->pml4_phys);

    pte_t pml4e = pml4[PML4_IDX(virt)];
    if (!(pml4e & PTE_PRESENT)) return 0;
    pte_t *pdp = (pte_t *)PHYS_TO_VIRT(PTE_CHILD_PHYS(pml4e));

    pte_t pdpe = pdp[PDP_IDX(virt)];
    if (!(pdpe & PTE_PRESENT)) return 0;
    pte_t *pd = (pte_t *)PHYS_TO_VIRT(PTE_CHILD_PHYS(pdpe));

    pte_t pde = pd[PD_IDX(virt)];
    if (!(pde & PTE_PRESENT)) return 0;
    if (pde & PTE_HUGE) /* 2 MiB page */
        return (PTE_CHILD_PHYS(pde) & ~((uint64_t)0x1FFFFF)) | (virt & 0x1FFFFF);
    pte_t *pt = (pte_t *)PHYS_TO_VIRT(PTE_CHILD_PHYS(pde));

    pte_t pte = pt[PT_IDX(virt)];
    if (!(pte & PTE_PRESENT)) return 0;
    return (PTE_CHILD_PHYS(pte)) | (virt & 0xFFF);
}

address_space_t *vmm_create_address_space(void)
{
    uint64_t phys = pmm_alloc_page();
    if (!phys) return NULL;
    memset(PHYS_TO_VIRT(phys), 0, PAGE_SIZE);

    address_space_t *as = (address_space_t *)PHYS_TO_VIRT(pmm_alloc_page());
    if (!as) { pmm_free_page(phys); return NULL; }
    as->pml4_phys = phys;

    /* Share the upper half of the kernel PML4 */
    pte_t *new_pml4    = (pte_t *)PHYS_TO_VIRT(phys);
    pte_t *kernel_pml4 = (pte_t *)PHYS_TO_VIRT(kernel_as.pml4_phys);
    for (int i = 256; i < 512; i++)
        new_pml4[i] = kernel_pml4[i];

    return as;
}

void vmm_destroy_address_space(address_space_t *as)
{
    if (!as) return;
    /* TODO: walk and free all user-space page tables */
    pmm_free_page(as->pml4_phys);
    pmm_free_page((uint64_t)vmm_virt_to_phys(NULL, (uint64_t)as));
}

void vmm_switch_address_space(address_space_t *as)
{
    if (!as) as = &kernel_as;
    __asm__ volatile("mov %0, %%cr3" : : "r"(as->pml4_phys) : "memory");
}

/* ── Kernel VMM initialisation ───────────────────────────────────────────── */
void vmm_init(void)
{
    /* Allocate a fresh PML4 for the kernel */
    uint64_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        /* Cannot proceed without page tables */
        __asm__ volatile("cli; hlt");
    }
    memset(PHYS_TO_VIRT(pml4_phys), 0, PAGE_SIZE);
    kernel_as.pml4_phys = pml4_phys;

    /* Identity map 0 – 4 GiB (2 MiB huge pages) */
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 0x200000ULL) {
        vmm_map_page(&kernel_as, addr, addr,
                     PTE_PRESENT | PTE_WRITABLE | PTE_HUGE | PTE_GLOBAL);
    }

    /* Direct physical map at PHYS_MAP_BASE (0 – 4 GiB with 2 MiB pages) */
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 0x200000ULL) {
        vmm_map_page(&kernel_as, PHYS_MAP_BASE + addr, addr,
                     PTE_PRESENT | PTE_WRITABLE | PTE_HUGE | PTE_GLOBAL | PTE_NO_EXEC);
    }

    /* Map kernel image (1 GiB region at KERNEL_VIRT_BASE, from physical 0) */
    for (uint64_t addr = 0; addr < 0x40000000ULL; addr += 0x200000ULL) {
        vmm_map_page(&kernel_as, KERNEL_VIRT_BASE + addr, addr,
                     PTE_PRESENT | PTE_WRITABLE | PTE_HUGE | PTE_GLOBAL);
    }

    vmm_switch_address_space(&kernel_as);
}
