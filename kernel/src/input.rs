//! The kernel input queue.
//!
//! Interrupt handlers push events; the desktop drains them.  The spin lock is
//! only ever taken with interrupts disabled, which makes it safe to use from
//! both contexts.

use zirvstdlib::abi::InputEvent;
use zirvstdlib::ring::RingBuffer;
use zirvstdlib::sync::SpinLock;

const CAPACITY: usize = 512;

static QUEUE: SpinLock<RingBuffer<InputEvent, CAPACITY>> =
    SpinLock::new(RingBuffer::new());

/// Push an event.  Called from interrupt context.
pub fn push(event: InputEvent) {
    let mut queue = QUEUE.lock();
    let _ = queue.push_overwrite(event);
}

/// Drain up to `output.len()` events into `output`, returning the count.
///
/// Interrupts are disabled while the queue is locked so that a timer or input
/// interrupt cannot spin forever on a lock held by the interrupted code.
pub fn drain_into(output: &mut [InputEvent]) -> usize {
    let were_enabled = crate::arch::cpu::interrupts_enabled();
    crate::arch::cpu::cli();
    let count = {
        let mut queue = QUEUE.lock();
        let mut count = 0;
        while count < output.len() {
            match queue.pop() {
                Some(event) => {
                    output[count] = event;
                    count += 1;
                }
                None => break,
            }
        }
        count
    };
    if were_enabled {
        crate::arch::cpu::sti();
    }
    count
}

/// Discard every queued event.
pub fn clear() {
    let were_enabled = crate::arch::cpu::interrupts_enabled();
    crate::arch::cpu::cli();
    {
        let mut queue = QUEUE.lock();
        queue.clear();
    }
    if were_enabled {
        crate::arch::cpu::sti();
    }
}

/// Number of queued events, mostly for diagnostics.
pub fn len() -> usize {
    let were_enabled = crate::arch::cpu::interrupts_enabled();
    crate::arch::cpu::cli();
    let length = QUEUE.lock().len();
    if were_enabled {
        crate::arch::cpu::sti();
    }
    length
}
