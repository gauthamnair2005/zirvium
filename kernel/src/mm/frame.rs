//! Physical frame allocator.
//!
//! A bitmap tracks 4 KiB frames over the first 64 GiB of physical address
//! space.  The boot memory map seeds the bitmap; everything the kernel is
//! actively using is reserved before the first allocation.

use core::ptr;
use zirvstdlib::mem::PhysAddr;
use zirvstdlib::sync::SpinLock;

use crate::uefi::{memory_type, MemoryDescriptor};

pub const FRAME_SIZE: u64 = 4096;
pub const MAX_FRAMES: usize = 16 * 1024 * 1024; // 64 GiB.
const BITMAP_WORDS: usize = MAX_FRAMES / 64;

static mut FRAME_BITMAP: [u64; BITMAP_WORDS] = [0; BITMAP_WORDS];

pub struct FrameAllocator {
    bitmap: &'static mut [u64],
    total_frames: usize,
    free_frames: usize,
    next_hint: usize,
}

static ALLOCATOR: SpinLock<Option<FrameAllocator>> = SpinLock::new(None);

impl FrameAllocator {
    fn new(bitmap: &'static mut [u64]) -> Self {
        for word in bitmap.iter_mut() {
            *word = u64::MAX;
        }
        Self {
            bitmap,
            total_frames: 0,
            free_frames: 0,
            next_hint: 0,
        }
    }

    #[inline]
    fn is_used(&self, frame: usize) -> bool {
        (self.bitmap[frame / 64] >> (frame % 64)) & 1 != 0
    }

    #[inline]
    fn set_used(&mut self, frame: usize, used: bool) {
        let mask = 1u64 << (frame % 64);
        let word = &mut self.bitmap[frame / 64];
        if used {
            *word |= mask;
        } else {
            *word &= !mask;
        }
    }

    fn mark_free(&mut self, start_frame: usize, end_frame: usize) {
        let end_frame = end_frame.min(MAX_FRAMES);
        for frame in start_frame..end_frame {
            if self.is_used(frame) {
                self.set_used(frame, false);
                self.free_frames += 1;
            }
        }
        self.total_frames = self.total_frames.max(end_frame);
    }

    pub fn reserve_range(&mut self, start: u64, end: u64) {
        let start_frame = (start / FRAME_SIZE) as usize;
        let end_frame = (end.div_ceil(FRAME_SIZE)) as usize;
        for frame in start_frame..end_frame.min(MAX_FRAMES) {
            if !self.is_used(frame) {
                self.set_used(frame, true);
                self.free_frames = self.free_frames.saturating_sub(1);
            }
        }
    }

    pub fn alloc_frame(&mut self) -> Option<PhysAddr> {
        if self.free_frames == 0 {
            return None;
        }
        for offset in 0..MAX_FRAMES {
            let frame = (self.next_hint + offset) % MAX_FRAMES;
            if !self.is_used(frame) {
                self.set_used(frame, true);
                self.free_frames -= 1;
                self.next_hint = (frame + 1) % MAX_FRAMES;
                return Some(PhysAddr::new(frame as u64 * FRAME_SIZE));
            }
        }
        None
    }

    pub fn alloc_contiguous(&mut self, count: usize) -> Option<PhysAddr> {
        if count == 0 || count > self.free_frames {
            return None;
        }
        let mut run_start = 0usize;
        let mut run_length = 0usize;
        for frame in 0..MAX_FRAMES {
            if self.is_used(frame) {
                run_length = 0;
                run_start = frame + 1;
            } else {
                if run_length == 0 {
                    run_start = frame;
                }
                run_length += 1;
                if run_length == count {
                    for allocated in run_start..run_start + count {
                        self.set_used(allocated, true);
                    }
                    self.free_frames -= count;
                    self.next_hint = (run_start + count) % MAX_FRAMES;
                    return Some(PhysAddr::new(run_start as u64 * FRAME_SIZE));
                }
            }
        }
        None
    }

    pub fn free_frame(&mut self, address: PhysAddr) {
        let frame = (address.0 / FRAME_SIZE) as usize;
        if frame < MAX_FRAMES && self.is_used(frame) {
            self.set_used(frame, false);
            self.free_frames += 1;
        }
    }

    pub fn free_contiguous(&mut self, address: PhysAddr, count: usize) {
        let start = (address.0 / FRAME_SIZE) as usize;
        for frame in start..start + count {
            if frame < MAX_FRAMES && self.is_used(frame) {
                self.set_used(frame, false);
                self.free_frames += 1;
            }
        }
    }

    pub fn free_frames(&self) -> usize {
        self.free_frames
    }

    pub fn total_frames(&self) -> usize {
        self.total_frames
    }

    pub fn free_bytes(&self) -> u64 {
        self.free_frames as u64 * FRAME_SIZE
    }

    pub fn total_bytes(&self) -> u64 {
        self.total_frames as u64 * FRAME_SIZE
    }
}

/// Initialise the allocator from the firmware memory map.
pub fn init<'a>(
    descriptors: impl Iterator<Item = &'a MemoryDescriptor>,
    kernel_start: u64,
    kernel_end: u64,
    stack_pointer: u64,
    framebuffer_start: u64,
    framebuffer_end: u64,
) {
    let bitmap = unsafe {
        core::slice::from_raw_parts_mut(
            ptr::addr_of_mut!(FRAME_BITMAP) as *mut u64,
            BITMAP_WORDS,
        )
    };
    let mut allocator = FrameAllocator::new(bitmap);

    for descriptor in descriptors {
        let usable = matches!(
            descriptor.memory_type,
            memory_type::CONVENTIONAL
                | memory_type::LOADER_CODE
                | memory_type::LOADER_DATA
                | memory_type::BOOT_SERVICES_CODE
                | memory_type::BOOT_SERVICES_DATA
                | memory_type::ACPI_RECLAIM
        );
        if usable && descriptor.number_of_pages > 0 {
            let start = (descriptor.physical_start / FRAME_SIZE) as usize;
            let end = ((descriptor.physical_start + descriptor.number_of_pages * FRAME_SIZE)
                / FRAME_SIZE) as usize;
            allocator.mark_free(start, end);
        }
    }

    // Never hand out the first megabyte: it contains firmware tables, the
    // legacy VGA window and, on some machines, the real-mode IVT.
    allocator.reserve_range(0, 0x10_0000);
    allocator.reserve_range(kernel_start, kernel_end);
    allocator.reserve_range(framebuffer_start, framebuffer_end);

    // Reserve the firmware stack.  We keep using it until a task stack is
    // installed; 1 MiB below and 64 KiB above the current pointer is a
    // conservative envelope.
    let stack_start = stack_pointer.saturating_sub(1024 * 1024);
    let stack_end = stack_pointer.saturating_add(64 * 1024);
    allocator.reserve_range(stack_start, stack_end);

    // The memory map buffer and frame bitmap sit inside the kernel image and
    // are already covered by the image reservation.

    let free = allocator.free_frames;
    let total = allocator.total_frames;
    *ALLOCATOR.lock() = Some(allocator);
    crate::sprintln!(
        "MM: {} MiB usable, {} MiB free",
        total * 4 / 1024,
        free * 4 / 1024
    );
}

pub fn alloc_frame() -> Option<PhysAddr> {
    ALLOCATOR.lock().as_mut()?.alloc_frame()
}

pub fn alloc_contiguous(count: usize) -> Option<PhysAddr> {
    ALLOCATOR.lock().as_mut()?.alloc_contiguous(count)
}

pub fn free_frame(address: PhysAddr) {
    if let Some(allocator) = ALLOCATOR.lock().as_mut() {
        allocator.free_frame(address);
    }
}

pub fn free_contiguous(address: PhysAddr, count: usize) {
    if let Some(allocator) = ALLOCATOR.lock().as_mut() {
        allocator.free_contiguous(address, count);
    }
}

pub fn stats() -> (u64, u64) {
    let lock = ALLOCATOR.lock();
    match lock.as_ref() {
        Some(allocator) => (allocator.free_bytes(), allocator.total_bytes()),
        None => (0, 0),
    }
}

pub fn reserve_range(start: u64, end: u64) {
    if let Some(allocator) = ALLOCATOR.lock().as_mut() {
        allocator.reserve_range(start, end);
    }
}
