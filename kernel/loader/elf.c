#include "elf.h"
#include "kernel/mm/vmm.h"
#include "kernel/mm/pmm.h"
#include "kernel/console.h"
#include <string.h>

/* ELF64 definitions */
typedef struct {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} elf64_header_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_ph_t;

#define PT_LOAD 1

static bool elf_validate(const void *buffer)
{
    const elf64_header_t *hdr = (const elf64_header_t *)buffer;
    if (memcmp(hdr->e_ident, "\x7f\x45\x4c\x46", 4) != 0) return false;
    if (hdr->e_ident[4] != 2) return false;
    return true;
}

bool elf_load_into_as(address_space_t *as, const void *buffer, uint64_t *entry)
{
    if (!elf_validate(buffer)) return false;

    const elf64_header_t *hdr = (const elf64_header_t *)buffer;
    const elf64_ph_t *ph = (const elf64_ph_t *)((uintptr_t)buffer + hdr->e_phoff);

    kprintf("[dbg] elf_load: phnum=%d\n", hdr->e_phnum);
    for (int i = 0; i < hdr->e_phnum; i++) {
        kprintf("[dbg] elf_load: seg%d p_type=%d vaddr=0x%lx memsz=0x%lx\n",
                i, ph[i].p_type, ph[i].p_vaddr, ph[i].p_memsz);
        if (ph[i].p_type == PT_LOAD) {
            uint64_t vaddr    = ph[i].p_vaddr;
            uint64_t size     = ph[i].p_memsz;
            uint64_t offset   = ph[i].p_offset;
            uint64_t filesz   = ph[i].p_filesz;

            /* ── Account for non-page-aligned vaddr ─────────────────────── */
            uint64_t page_off = vaddr & (PAGE_SIZE - 1);   /* offset within first page */
            uint64_t base_va  = vaddr & ~(PAGE_SIZE - 1);  /* page-aligned base */

            uint64_t pages = (page_off + size + PAGE_SIZE - 1) / PAGE_SIZE;
            kprintf("[dbg] elf_load: seg%d pages=%lu (page_off=0x%lx)\n", i, pages, page_off);
            for (uint64_t p = 0; p < pages; p++) {
                uintptr_t page_va = base_va + (p * PAGE_SIZE);
                uint64_t phys = pmm_alloc_page();
                if (!phys) { kprintf("[dbg] elf_load: OOM p=%lu\n", p); return false; }

                uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_USER;
                vmm_map_page(as, page_va, phys, flags);

                void *page_ka = PHYS_TO_VIRT(phys);
                memset(page_ka, 0, PAGE_SIZE);

                /* source offset in file for this page's initialized data */
                int64_t rel_page_start = (int64_t)(p * PAGE_SIZE) - (int64_t)page_off;
                if (rel_page_start < (int64_t)filesz) {
                    uint64_t dest_off   = (p == 0) ? page_off : 0;
                    uint64_t remain     = filesz - (uint64_t)(rel_page_start < 0 ? 0 : rel_page_start);
                    uint64_t copy_size  = (PAGE_SIZE - dest_off) < remain ? (PAGE_SIZE - dest_off) : remain;
                    uint64_t src_off    = offset + (uint64_t)(rel_page_start < 0 ? 0 : rel_page_start);

                    memcpy(page_ka + dest_off, (void *)((uintptr_t)buffer + src_off), copy_size);
                }
            }
        }
        kprintf("[dbg] elf_load: seg%d done\n", i);
    }

    if (entry) *entry = hdr->e_entry;
    return true;
}

bool elf_load_process(process_t *proc, const void *buffer)
{
    return elf_load_into_as(proc->as, buffer, &proc->user_rip);
}