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

/* Physical address mask for a 2 MiB huge-page PDE (bits [51:21]) */
#define PTE_2MIB_PHYS_MASK  0x000FFFFFFFE00000ULL

static address_space_t kernel_as;

/* ── Early-init helpers (before PHYS_MAP is active) ──────────────────────── */

/*
 * During early boot the CPU is still using the boot page-tables, which
 * provide an identity map (PA == VA) for the first 4 GiB of physical memory.
 * PHYS_TO_VIRT() adds PHYS_MAP_BASE and therefore cannot be used until the
 * new page tables are loaded.  These helpers use the identity map instead.
 */
static inline pte_t *early_identity_ptr(uint64_t phys)
{
    return (pte_t *)(uintptr_t)phys;
}

static uint64_t early_alloc_zeroed(void)
{
    uint64_t phys = pmm_alloc_page();
    if (!phys) {
        __asm__ volatile("cli; hlt");
        for (;;) __asm__ volatile("hlt");
    }
    memset(early_identity_ptr(phys), 0, PAGE_SIZE);
    return phys;
}

/*
 * Map a single 2 MiB huge page into pml4 (accessed via identity map).
 * flags should include PTE_WRITABLE, PTE_GLOBAL, PTE_NO_EXEC as needed;
 * PTE_PRESENT and PTE_HUGE are always added by this function.
 */
static void early_map_2m(pte_t *pml4, uint64_t virt, uint64_t phys,
                          uint64_t flags)
{
    const uint64_t tbl_flags = PTE_PRESENT | PTE_WRITABLE;

    if (!(pml4[PML4_IDX(virt)] & PTE_PRESENT))
        pml4[PML4_IDX(virt)] = early_alloc_zeroed() | tbl_flags;
    pte_t *pdp = early_identity_ptr(PTE_CHILD_PHYS(pml4[PML4_IDX(virt)]));

    if (!(pdp[PDP_IDX(virt)] & PTE_PRESENT))
        pdp[PDP_IDX(virt)] = early_alloc_zeroed() | tbl_flags;
    pte_t *pd = early_identity_ptr(PTE_CHILD_PHYS(pdp[PDP_IDX(virt)]));

    pd[PD_IDX(virt)] = (phys & PTE_2MIB_PHYS_MASK) | flags | PTE_PRESENT | PTE_HUGE;
}

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

    if (flags & PTE_HUGE) {
        /* 2 MiB page: set the PDE directly — no PT needed */
        pd[PD_IDX(virt)] = (phys & PTE_2MIB_PHYS_MASK) | flags | PTE_PRESENT;
        __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
        return;
    }

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
    /*
     * At this point the boot page-tables (from boot.asm) are still active.
     * They provide only an identity map of 0–4 GiB and the kernel higher-half
     * mapping.  PHYS_MAP_BASE is NOT yet mapped, so PHYS_TO_VIRT() must not
     * be used until after vmm_switch_address_space() below.
     *
     * We build the new kernel page tables using early_* helpers that access
     * physical pages through the identity map (PA == VA).
     */
    uint64_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        /* Cannot proceed without page tables */
        __asm__ volatile("cli; hlt");
        for (;;) __asm__ volatile("hlt");
    }
    memset(early_identity_ptr(pml4_phys), 0, PAGE_SIZE);
    kernel_as.pml4_phys = pml4_phys;
    pte_t *pml4 = early_identity_ptr(pml4_phys);

    /* Identity map 0 – 4 GiB (2 MiB huge pages) */
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 0x200000ULL)
        early_map_2m(pml4, addr, addr,
                     PTE_WRITABLE | PTE_GLOBAL);

    /* Direct physical map at PHYS_MAP_BASE (0 – 4 GiB with 2 MiB pages) */
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 0x200000ULL)
        early_map_2m(pml4, PHYS_MAP_BASE + addr, addr,
                     PTE_WRITABLE | PTE_GLOBAL | PTE_NO_EXEC);

    /* Map kernel image (1 GiB region at KERNEL_VIRT_BASE, from physical KERNEL_PHYS_BASE) */
    for (uint64_t addr = 0; addr < 0x40000000ULL; addr += 0x200000ULL)
        early_map_2m(pml4, KERNEL_VIRT_BASE + addr, KERNEL_PHYS_BASE + addr,
                     PTE_WRITABLE | PTE_GLOBAL);

    /*
     * Switch to the new page tables.  From this point on PHYS_TO_VIRT() is
     * valid and vmm_map_page() / get_or_alloc_table() can be used normally.
     */
    vmm_switch_address_space(&kernel_as);
}
