//! Identity-mapped paging.
//!
//! UEFI hands us a set of page tables that are not guaranteed to cover all of
//! RAM or all MMIO.  We build a simple, predictable 64 GiB identity map using
//! 2 MiB huge pages.  This covers every PCI BAR and framebuffer address we are
//! likely to encounter on x86-64, and keeps physical == virtual, which makes
//! driver DMA setup trivial.  Finer-grained mappings and userspace address
//! spaces can be layered on later.

use crate::arch::cpu;
use core::arch::asm;

const PRESENT: u64 = 1 << 0;
const WRITABLE: u64 = 1 << 1;
const HUGE: u64 = 1 << 7;
const GLOBAL: u64 = 1 << 8;

const TABLE_ENTRIES: usize = 512;
const PDPT_ENTRIES: usize = 64; // 64 GiB of 1 GiB entries.
pub const MAPPED_SIZE: u64 = 64 * 1024 * 1024 * 1024;

#[derive(Clone, Copy)]
#[repr(C, align(4096))]
struct PageTable {
    entries: [u64; TABLE_ENTRIES],
}

impl PageTable {
    const fn zeroed() -> Self {
        Self {
            entries: [0; TABLE_ENTRIES],
        }
    }
}

static mut PML4: PageTable = PageTable::zeroed();
static mut PDPT: PageTable = PageTable::zeroed();
static mut PD: [PageTable; PDPT_ENTRIES] = [PageTable::zeroed(); PDPT_ENTRIES];

/// Build and install the identity map.
pub fn init() {
    // SAFETY: called once during early boot with interrupts disabled.  The
    // tables are static and only mutated here.
    unsafe {
        let pml4 = &raw mut PML4;
        let pdpt = &raw mut PDPT;
        let pd = &raw mut PD;

        (*pml4).entries[0] = pdpt as u64 | PRESENT | WRITABLE;
        for index in 0..PDPT_ENTRIES {
            let table = (*pd).as_mut_ptr().add(index) as u64;
            (*pdpt).entries[index] = table | PRESENT | WRITABLE;
            for entry in 0..TABLE_ENTRIES {
                let physical = ((index * TABLE_ENTRIES + entry) as u64) << 21;
                (*pd)[index].entries[entry] = physical | PRESENT | WRITABLE | HUGE | GLOBAL;
            }
        }

        cpu::write_cr3(pml4 as u64);
        asm!("mfence", options(nostack, preserves_flags));

        // Enable write protection in ring 0 so read-only pages are honoured.
        let mut cr0 = cpu::read_cr0();
        cr0 |= 1 << 16;
        cpu::write_cr0(cr0);
    }
}

/// The physical address of the root table.
pub fn root_table() -> u64 {
    &raw const PML4 as u64
}

pub fn is_mapped(address: u64) -> bool {
    address < MAPPED_SIZE
}

/// Mark the 2 MiB pages covering `[start, end)` as uncached.
pub fn mark_uncached(start: u64, end: u64) {
    let start_index = (start / (2 * 1024 * 1024)) as usize;
    let end_index = (end.div_ceil(2 * 1024 * 1024)) as usize;
    // SAFETY: the tables are static and this is called during boot with a
    // single CPU.
    unsafe {
        let pd = &raw mut PD;
        for index in start_index..end_index.min(PDPT_ENTRIES * TABLE_ENTRIES) {
            let table = index / TABLE_ENTRIES;
            let entry = index % TABLE_ENTRIES;
            (*pd)[table].entries[entry] |= (1 << 4) | (1 << 3); // PCD | PWT
            // The mappings use the GLOBAL bit, so reloading CR3 does not
            // invalidate them.  Invalidate each page explicitly.
            core::arch::asm!(
                "invlpg [{}]",
                in(reg) index as u64 * 2 * 1024 * 1024,
                options(nostack, preserves_flags)
            );
        }
    }
}

pub fn flush_tlb() {
    // Reloading CR3 flushes the non-global TLB entries.
    let root = cpu::read_cr3();
    cpu::write_cr3(root);
}
