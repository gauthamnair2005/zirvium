//! Kernel heap.
//!
//! A boundary-tagged, address-ordered free-list allocator.  Every block
//! begins on a 16-byte boundary and carries a small header.  Free blocks are
//! kept in one address-sorted list so that neighbouring blocks coalesce
//! immediately.  The allocator is deliberately simple: it only has to serve
//! the kernel, the compositor and the desktop, all of which have predictable
//! allocation patterns.
//!
//! The header is placed immediately before the returned pointer.  This makes
//! deallocation a subtraction and lets the allocator support alignments
//! greater than 16 without a lookup table.

use core::alloc::{GlobalAlloc, Layout};
use core::ptr;

use zirvstdlib::math::align_up;
use zirvstdlib::sync::SpinLock;

/// Size of a block header.  Must be a multiple of 16 so that a block aligned
/// to 16 keeps the user pointer aligned to 16.
const HEADER_SIZE: usize = 48;
/// Never create a free block smaller than this; the header plus a useful
/// payload would not fit.
const MIN_BLOCK: usize = 64;
/// Alignment of every block.
const BLOCK_ALIGN: usize = 16;

#[repr(C)]
struct Header {
    /// Total size of the block, including alignment padding and the header.
    size: usize,
    /// Distance from the block start to this header.  Non-zero only when the
    /// user requested an alignment greater than 16.
    base: usize,
    next: *mut Header,
    prev: *mut Header,
    free: bool,
    _padding: [u8; 7],
    _reserved: usize,
}

const _: () = assert!(core::mem::size_of::<Header>() == HEADER_SIZE);

pub struct Heap {
    start: usize,
    end: usize,
    free_list: *mut Header,
    free_bytes: usize,
    total_bytes: usize,
}

// SAFETY: the heap is only reachable through the global spin lock below, and
// the raw pointers refer to the statically owned heap region.
unsafe impl Send for Heap {}

impl Heap {
    const fn empty() -> Self {
        Self {
            start: 0,
            end: 0,
            free_list: ptr::null_mut(),
            free_bytes: 0,
            total_bytes: 0,
        }
    }

    /// Build a heap covering `[start, end)`.
    ///
    /// # Safety
    /// The region must be exclusively owned by the kernel and identity mapped.
    pub unsafe fn new(start: usize, end: usize) -> Self {
        let start = align_up(start, BLOCK_ALIGN);
        let end = end & !(BLOCK_ALIGN - 1);
        let mut heap = Self::empty();
        if end <= start + MIN_BLOCK {
            return heap;
        }
        heap.start = start;
        heap.end = end;
        let size = end - start;
        // SAFETY: the region is owned by the caller and large enough for a
        // header.
        unsafe {
            let header = start as *mut Header;
            header.write(Header {
                size,
                base: 0,
                next: ptr::null_mut(),
                prev: ptr::null_mut(),
                free: true,
                _padding: [0; 7],
                _reserved: 0,
            });
        }
        heap.free_list = start as *mut Header;
        heap.free_bytes = size;
        heap.total_bytes = size;
        heap
    }

    #[inline]
    fn align_size(size: usize) -> usize {
        align_up(size.max(1), BLOCK_ALIGN)
    }

    /// Insert a free block into the address-ordered list, coalescing with its
    /// neighbours.
    ///
    /// # Safety
    /// `header` must point to a valid, initialised header inside the heap and
    /// must not already be in the list.
    unsafe fn insert_free(&mut self, header: *mut Header) {
        // SAFETY: caller guarantees the header is valid.
        unsafe {
            (*header).free = true;
            (*header).next = ptr::null_mut();
            (*header).prev = ptr::null_mut();

            let address = header as usize;
            let mut previous: *mut Header = ptr::null_mut();
            let mut current = self.free_list;
            while !current.is_null() && (current as usize) < address {
                previous = current;
                current = (*current).next;
            }

            (*header).prev = previous;
            (*header).next = current;
            if previous.is_null() {
                self.free_list = header;
            } else {
                (*previous).next = header;
            }
            if !current.is_null() {
                (*current).prev = header;
            }

            self.coalesce(header);
        }
    }

    /// Merge `header` with its immediate neighbours when they are adjacent.
    ///
    /// # Safety
    /// `header` must be a free block currently linked into the list.
    unsafe fn coalesce(&mut self, header: *mut Header) {
        // SAFETY: the caller guarantees the header is linked and valid.
        unsafe {
            let end = header as usize + (*header).size;

            // Merge with the following block.
            let next = (*header).next;
            if !next.is_null() && next as usize == end {
                (*header).size += (*next).size;
                (*header).next = (*next).next;
                if !(*next).next.is_null() {
                    (*(*next).next).prev = header;
                }
            }

            // Merge with the preceding block.
            let previous = (*header).prev;
            if !previous.is_null() && previous as usize + (*previous).size == header as usize {
                (*previous).size += (*header).size;
                (*previous).next = (*header).next;
                if !(*header).next.is_null() {
                    (*(*header).next).prev = previous;
                }
            }
        }
    }

    /// Allocate `size` bytes with the requested alignment.
    unsafe fn allocate(&mut self, size: usize, alignment: usize) -> *mut u8 {
        let size = Self::align_size(size);
        let alignment = alignment.max(BLOCK_ALIGN);

        let mut current = self.free_list;
        while !current.is_null() {
            // SAFETY: list nodes are valid headers by construction.
            unsafe {
                let block_start = current as usize;
                let block_end = block_start + (*current).size;
                // The user pointer lives as far into the block as the
                // alignment requires, with the header immediately before it.
                let data = align_up(block_start + HEADER_SIZE, alignment);
                let required_end = match data.checked_add(size) {
                    Some(value) => value,
                    None => return ptr::null_mut(),
                };
                if required_end > block_end {
                    current = (*current).next;
                    continue;
                }

                let next = (*current).next;
                let previous = (*current).prev;

                // Decide whether the tail is worth keeping as its own block.
                let tail_start = align_up(required_end, BLOCK_ALIGN);
                let tail_size = block_end - tail_start;
                let block_size = if tail_size >= MIN_BLOCK {
                    tail_start - block_start
                } else {
                    block_end - block_start
                };

                // Unlink the block from the free list.
                if previous.is_null() {
                    self.free_list = next;
                } else {
                    (*previous).next = next;
                }
                if !next.is_null() {
                    (*next).prev = previous;
                }

                // Write the allocation header immediately before the data.
                let header = (data - HEADER_SIZE) as *mut Header;
                header.write(Header {
                    size: block_size,
                    base: header as usize - block_start,
                    next: ptr::null_mut(),
                    prev: ptr::null_mut(),
                    free: false,
                    _padding: [0; 7],
                    _reserved: 0,
                });

                if tail_size >= MIN_BLOCK {
                    let tail = tail_start as *mut Header;
                    tail.write(Header {
                        size: tail_size,
                        base: 0,
                        next: ptr::null_mut(),
                        prev: ptr::null_mut(),
                        free: true,
                        _padding: [0; 7],
                        _reserved: 0,
                    });
                    self.insert_free(tail);
                }

                self.free_bytes = self.free_bytes.saturating_sub(block_size);
                return data as *mut u8;
            }
        }
        ptr::null_mut()
    }

    /// Return an allocation to the heap.
    ///
    /// # Safety
    /// `pointer` must have been returned by `allocate` and not freed since.
    unsafe fn deallocate(&mut self, pointer: *mut u8) {
        if pointer.is_null() {
            return;
        }
        // SAFETY: the header lives immediately before the user pointer.
        unsafe {
            let header = (pointer as usize - HEADER_SIZE) as *mut Header;
            let base = (*header).base;
            let size = (*header).size;
            let block_start = header as usize - base;
            if block_start < self.start || block_start + size > self.end {
                return;
            }
            // Recreate the header at the true block start so coalescing sees
            // the full extent, including any alignment padding.
            let block = block_start as *mut Header;
            block.write(Header {
                size,
                base: 0,
                next: ptr::null_mut(),
                prev: ptr::null_mut(),
                free: true,
                _padding: [0; 7],
                _reserved: 0,
            });
            self.free_bytes += size;
            self.insert_free(block);
        }
    }

    pub fn used_bytes(&self) -> usize {
        self.total_bytes.saturating_sub(self.free_bytes)
    }

    pub fn total_bytes(&self) -> usize {
        self.total_bytes
    }

    pub fn free_bytes(&self) -> usize {
        self.free_bytes
    }
}

static HEAP: SpinLock<Heap> = SpinLock::new(Heap::empty());

/// Initialise the kernel heap over a contiguous physical region.
///
/// # Safety
/// `start..end` must be exclusively owned by the kernel.
pub unsafe fn init(start: usize, end: usize) {
    // SAFETY: delegated to the caller.
    let heap = unsafe { Heap::new(start, end) };
    let total = heap.total_bytes;
    *HEAP.lock() = heap;
    crate::sprintln!("HEAP: {total} bytes at {start:#x}");
}

pub fn stats() -> (usize, usize) {
    let heap = HEAP.lock();
    (heap.used_bytes(), heap.total_bytes())
}

pub struct KernelAllocator;

unsafe impl GlobalAlloc for KernelAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        // SAFETY: the lock serialises access to the heap.
        let mut heap = HEAP.lock();
        // SAFETY: the heap region is valid and owned by the kernel.
        unsafe { heap.allocate(layout.size(), layout.align()) }
    }

    unsafe fn dealloc(&self, pointer: *mut u8, _layout: Layout) {
        // SAFETY: the lock serialises access and the pointer came from us.
        let mut heap = HEAP.lock();
        unsafe { heap.deallocate(pointer) }
    }

    unsafe fn realloc(&self, pointer: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
        // SAFETY: allocate and deallocate have the same preconditions as the
        // trait contract.
        unsafe {
            let new_pointer =
                self.alloc(Layout::from_size_align_unchecked(new_size, layout.align()));
            if !new_pointer.is_null() {
                let copied = layout.size().min(new_size);
                ptr::copy_nonoverlapping(pointer, new_pointer, copied);
                self.dealloc(pointer, layout);
            }
            new_pointer
        }
    }
}
