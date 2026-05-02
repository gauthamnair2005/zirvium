/* kernel/mm/vmm.h
 * Zirvium Kernel — Virtual Memory Manager
 *
 * Manages x86-64 4-level paging (PML4 → PDP → PD → PT).
 * The kernel is mapped at KERNEL_VIRT_BASE (0xFFFFFFFF80000000) and the
 * entire physical memory is also identity-mapped at PHYS_MAP_BASE
 * (0xFFFF800000000000) for easy physical ↔ virtual translation.
 */
#ifndef ZIRVIUM_KERNEL_MM_VMM_H
#define ZIRVIUM_KERNEL_MM_VMM_H

#include <stdint.h>
#include <stddef.h>

/* ── Virtual address layout ──────────────────────────────────────────────── */
#define KERNEL_VIRT_BASE    0xFFFFFFFF80000000ULL   /* kernel image base */
#define PHYS_MAP_BASE       0xFFFF800000000000ULL   /* direct physical map */

/* Convert a kernel virtual address to the physical address it was linked at */
#define KVIRT_TO_PHYS(v)    ((uint64_t)(v) - KERNEL_VIRT_BASE)
/* Convert a physical address to its direct-map virtual address */
#define PHYS_TO_VIRT(p)     ((void *)((uintptr_t)(p) + PHYS_MAP_BASE))

/* ── Page table entry flags ──────────────────────────────────────────────── */
#define PTE_PRESENT         (1ULL << 0)
#define PTE_WRITABLE        (1ULL << 1)
#define PTE_USER            (1ULL << 2)
#define PTE_WRITE_THROUGH   (1ULL << 3)
#define PTE_NO_CACHE        (1ULL << 4)
#define PTE_ACCESSED        (1ULL << 5)
#define PTE_DIRTY           (1ULL << 6)
#define PTE_HUGE            (1ULL << 7)  /* 2 MiB page (in PD) */
#define PTE_GLOBAL          (1ULL << 8)
#define PTE_NO_EXEC         (1ULL << 63) /* NX bit */

/* Flags for kernel mappings */
#define PTE_KERNEL_CODE  (PTE_PRESENT | PTE_GLOBAL)
#define PTE_KERNEL_DATA  (PTE_PRESENT | PTE_WRITABLE | PTE_GLOBAL | PTE_NO_EXEC)

/* ── Page-map type ────────────────────────────────────────────────────────── */
typedef uint64_t pte_t;

/* ── Address space handle ─────────────────────────────────────────────────── */
typedef struct {
    uint64_t pml4_phys;   /* physical address of the PML4 table */
} address_space_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * vmm_init - set up kernel virtual memory and switch to the full page tables.
 *
 * Must be called after pmm_init().  It rebuilds the page tables created by
 * the boot stub so they include:
 *  - identity mapping of the first 4 GiB (for early MMIO / ACPI)
 *  - physical direct-map at PHYS_MAP_BASE
 *  - kernel image at KERNEL_VIRT_BASE
 */
void vmm_init(void);

/**
 * vmm_map_page - map one virtual page to a physical frame.
 * @as:    address space (NULL → use kernel address space)
 * @virt:  page-aligned virtual address
 * @phys:  page-aligned physical address
 * @flags: PTE_* flags
 */
void vmm_map_page(address_space_t *as, uint64_t virt, uint64_t phys,
                  uint64_t flags);

/**
 * vmm_unmap_page - remove a virtual-to-physical mapping.
 * @as:   address space (NULL → kernel)
 * @virt: page-aligned virtual address
 */
void vmm_unmap_page(address_space_t *as, uint64_t virt);

/**
 * vmm_virt_to_phys - translate a virtual address to its physical address.
 * Returns 0 if the address is not mapped.
 */
uint64_t vmm_virt_to_phys(address_space_t *as, uint64_t virt);

/**
 * vmm_create_address_space - allocate and initialise a new user address space.
 * The kernel portion of the PML4 is shared with the kernel address space.
 */
address_space_t *vmm_create_address_space(void);

/**
 * vmm_destroy_address_space - free all page tables and the address_space_t.
 */
void vmm_destroy_address_space(address_space_t *as);

/**
 * vmm_switch_address_space - load an address space into CR3.
 */
void vmm_switch_address_space(address_space_t *as);

#endif /* ZIRVIUM_KERNEL_MM_VMM_H */
