//! Memory management: physical frames and the kernel heap.

pub mod frame;
pub mod heap;

use crate::uefi::{BootInfo, memory_type};
use zirvstdlib::mem::PhysAddr;

/// Size of the initial kernel heap.
pub const HEAP_SIZE: u64 = 96 * 1024 * 1024;

/// Initialise the physical frame allocator from the firmware memory map.
pub fn init(boot: &BootInfo, stack_pointer: u64) {
    let framebuffer_start = boot.framebuffer.address as u64;
    let framebuffer_end = framebuffer_start + boot.framebuffer.size;
    frame::init(
        boot.descriptors(),
        boot.image_base,
        boot.image_base + boot.image_size,
        stack_pointer,
        framebuffer_start,
        framebuffer_end,
    );
}

/// Allocate and install the kernel heap.
pub fn init_heap() {
    let frames = (HEAP_SIZE / frame::FRAME_SIZE) as usize;
    let base = match frame::alloc_contiguous(frames) {
        Some(address) => address,
        None => {
            crate::sprintln!("MM: unable to reserve a contiguous heap, shrinking");
            // Try progressively smaller heaps before giving up.
            let mut try_frames = frames / 2;
            loop {
                if try_frames < 256 {
                    panic!("out of memory: cannot create a kernel heap");
                }
                if let Some(address) = frame::alloc_contiguous(try_frames) {
                    // SAFETY: the frames were just reserved for the heap.
                    unsafe {
                        heap::init(
                            address.0 as usize,
                            (address.0 + try_frames as u64 * frame::FRAME_SIZE) as usize,
                        );
                    }
                    return;
                }
                try_frames /= 2;
            }
        }
    };
    // SAFETY: the frames were just reserved for the heap.
    unsafe {
        heap::init(base.0 as usize, (base.0 + HEAP_SIZE) as usize);
    }
}

/// Describe a memory type for diagnostics.
pub fn memory_type_name(value: u32) -> &'static str {
    match value {
        memory_type::RESERVED => "reserved",
        memory_type::LOADER_CODE => "loader code",
        memory_type::LOADER_DATA => "loader data",
        memory_type::BOOT_SERVICES_CODE => "boot services code",
        memory_type::BOOT_SERVICES_DATA => "boot services data",
        memory_type::RUNTIME_SERVICES_CODE => "runtime services code",
        memory_type::RUNTIME_SERVICES_DATA => "runtime services data",
        memory_type::CONVENTIONAL => "conventional",
        memory_type::UNUSABLE => "unusable",
        memory_type::ACPI_RECLAIM => "ACPI reclaim",
        memory_type::ACPI_NVS => "ACPI NVS",
        memory_type::MMIO => "MMIO",
        memory_type::MMIO_PORT => "MMIO port",
        memory_type::PERSISTENT => "persistent",
        memory_type::UNACCEPTED => "unaccepted",
        _ => "unknown",
    }
}

/// A snapshot used by the system monitor.
#[derive(Clone, Copy, Debug, Default)]
pub struct MemoryStats {
    pub free_bytes: u64,
    pub total_bytes: u64,
    pub heap_used: usize,
    pub heap_total: usize,
}

pub fn stats() -> MemoryStats {
    let (free, total) = frame::stats();
    let (heap_used, heap_total) = heap::stats();
    MemoryStats {
        free_bytes: free,
        total_bytes: total,
        heap_used,
        heap_total,
    }
}

/// Keep the physical address type visible to the rest of the kernel.
pub type PhysicalAddress = PhysAddr;
