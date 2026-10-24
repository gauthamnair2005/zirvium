//! Minimal E1000-compatible controller bring-up.
//!
//! This initializes the safe controller state and reads the programmed MAC.
//! Packet DMA is kept behind the network service until contiguous DMA buffers
//! and ownership fencing are installed.

use core::sync::atomic::{AtomicUsize, Ordering};
use zirvstdlib::sync::Once;

use super::pci::PciDevice;

const CTRL: usize = 0x0000;
const STATUS: usize = 0x0008;
const RCTL: usize = 0x0100;
const TCTL: usize = 0x0400;
const TIPG: usize = 0x0410;
const IMS: usize = 0x00d0;
const IMC: usize = 0x00d8;
const RDBAL: usize = 0x2800;
const RDBAH: usize = 0x2804;
const RDLEN: usize = 0x2808;
const RDH: usize = 0x2810;
const RDT: usize = 0x2818;
const TDBAL: usize = 0x3800;
const TDBAH: usize = 0x3804;
const TDLEN: usize = 0x3808;
const TDH: usize = 0x3810;
const TDT: usize = 0x3818;
const RAL: usize = 0x5400;
const RAH: usize = 0x5404;
const CTRL_RST: u32 = 1 << 26;
const STATUS_LU: u32 = 1 << 1;
const RCTL_EN: u32 = 1;
const RCTL_BAM: u32 = 1 << 15;
const TCTL_EN: u32 = 1;
const TCTL_PSP: u32 = 1 << 1;
const TXD_DONE: u8 = 1;
const RXD_DONE: u8 = 1;
const RXD_EOP: u8 = 2;
const RING_SIZE: usize = 16;
const BUFFER_SIZE: usize = 2048;

#[repr(C, align(16))]
#[derive(Clone, Copy)]
struct Descriptor {
    address: u64,
    length: u16,
    checksum: u16,
    status: u8,
    errors: u8,
    special: u16,
}

#[repr(C, align(16))]
struct DmaState {
    rx_descriptors: [Descriptor; RING_SIZE],
    tx_descriptors: [Descriptor; RING_SIZE],
    rx_buffers: [[u8; BUFFER_SIZE]; RING_SIZE],
    tx_buffers: [[u8; BUFFER_SIZE]; RING_SIZE],
}

impl DmaState {
    const fn empty() -> Self {
        let descriptor = Descriptor {
            address: 0,
            length: 0,
            checksum: 0,
            status: 0,
            errors: 0,
            special: 0,
        };
        Self {
            rx_descriptors: [descriptor; RING_SIZE],
            tx_descriptors: [descriptor; RING_SIZE],
            rx_buffers: [[0; BUFFER_SIZE]; RING_SIZE],
            tx_buffers: [[0; BUFFER_SIZE]; RING_SIZE],
        }
    }
}

static mut DMA: DmaState = DmaState::empty();
static TX_INDEX: AtomicUsize = AtomicUsize::new(0);

#[derive(Clone, Copy, Debug)]
pub struct E1000State {
    pub ready: bool,
    pub link_up: bool,
    pub mac: [u8; 6],
    pub mmio: u64,
}

static STATE: Once<E1000State> = Once::new();

unsafe fn read(base: u64, offset: usize) -> u32 {
    // SAFETY: the BAR is selected from a PCI network controller and the
    // register offset is within the E1000 register aperture.
    unsafe { core::ptr::read_volatile((base as usize + offset) as *const u32) }
}

unsafe fn write(base: u64, offset: usize, value: u32) {
    // SAFETY: same BAR and register constraints as `read`.
    unsafe { core::ptr::write_volatile((base as usize + offset) as *mut u32, value) };
}

fn bar_base(device: &PciDevice) -> Option<u64> {
    device.memory_bar().map(|(base, _)| base)
}

pub fn init(device: &PciDevice) -> E1000State {
    if let Some(existing) = STATE.get().copied() {
        return existing;
    }
    let Some(base) = bar_base(device) else {
        return *STATE.call_once(|| E1000State {
            ready: false,
            link_up: false,
            mac: [0; 6],
            mmio: 0,
        });
    };
    let state = unsafe {
        write(base, IMC, u32::MAX);
        write(base, CTRL, CTRL_RST);
        for _ in 0..10_000 {
            core::hint::spin_loop();
        }
        let ral = read(base, RAL);
        let rah = read(base, RAH);
        let dma = &mut *core::ptr::addr_of_mut!(DMA);
        for index in 0..RING_SIZE {
            dma.rx_descriptors[index].address = dma.rx_buffers[index].as_ptr() as u64;
            dma.rx_descriptors[index].status = 0;
            dma.tx_descriptors[index].address = dma.tx_buffers[index].as_ptr() as u64;
            dma.tx_descriptors[index].status = TXD_DONE;
        }
        write(base, RDBAL, dma.rx_descriptors.as_ptr() as u32);
        write(base, RDBAH, (dma.rx_descriptors.as_ptr() as u64 >> 32) as u32);
        write(base, RDLEN, (core::mem::size_of::<Descriptor>() * RING_SIZE) as u32);
        write(base, RDH, 0);
        write(base, RDT, (RING_SIZE - 1) as u32);
        write(base, TDBAL, dma.tx_descriptors.as_ptr() as u32);
        write(base, TDBAH, (dma.tx_descriptors.as_ptr() as u64 >> 32) as u32);
        write(base, TDLEN, (core::mem::size_of::<Descriptor>() * RING_SIZE) as u32);
        write(base, TDH, 0);
        write(base, TDT, 0);
        write(base, RCTL, RCTL_EN | RCTL_BAM);
        write(base, TCTL, TCTL_EN | TCTL_PSP | (0x10 << 4) | (0x40 << 12));
        write(base, TIPG, 0x0060_100a);
        write(base, IMS, 0);
        E1000State {
            ready: true,
            link_up: read(base, STATUS) & STATUS_LU != 0,
            mac: [
                (ral & 0xff) as u8,
                ((ral >> 8) & 0xff) as u8,
                ((ral >> 16) & 0xff) as u8,
                ((ral >> 24) & 0xff) as u8,
                (rah & 0xff) as u8,
                ((rah >> 8) & 0xff) as u8,
            ],
            mmio: base,
        }
    };
    STATE.call_once(|| state);
    state
}

pub fn state() -> Option<E1000State> {
    STATE.get().copied()
}

pub fn transmit(frame: &[u8]) -> Result<(), &'static str> {
    let state = STATE.get().copied().ok_or("E1000 is not initialized")?;
    if frame.is_empty() || frame.len() > BUFFER_SIZE {
        return Err("invalid Ethernet frame length");
    }
    let index = TX_INDEX.fetch_add(1, Ordering::AcqRel) % RING_SIZE;
    let dma = unsafe { &mut *core::ptr::addr_of_mut!(DMA) };
    if dma.tx_descriptors[index].status & TXD_DONE == 0 {
        return Err("E1000 transmit ring is busy");
    }
    dma.tx_buffers[index][..frame.len()].copy_from_slice(frame);
    dma.tx_descriptors[index].length = frame.len() as u16;
    dma.tx_descriptors[index].status = 0;
    core::sync::atomic::fence(Ordering::Release);
    unsafe { write(state.mmio, TDT, ((index + 1) % RING_SIZE) as u32) };
    for _ in 0..100_000 {
        if dma.tx_descriptors[index].status & TXD_DONE != 0 {
            return Ok(());
        }
        core::hint::spin_loop();
    }
    Err("E1000 transmit timeout")
}

pub fn receive(buffer: &mut [u8]) -> Option<usize> {
    let state = STATE.get().copied()?;
    let dma = unsafe { &mut *core::ptr::addr_of_mut!(DMA) };
    let head = unsafe { read(state.mmio, RDH) as usize % RING_SIZE };
    let descriptor = &mut dma.rx_descriptors[head];
    if descriptor.status & RXD_DONE == 0 {
        return None;
    }
    let end_of_packet = descriptor.status & RXD_EOP != 0;
    let length = (descriptor.length as usize).min(buffer.len()).min(BUFFER_SIZE);
    buffer[..length].copy_from_slice(&dma.rx_buffers[head][..length]);
    descriptor.status = 0;
    descriptor.length = 0;
    core::sync::atomic::fence(Ordering::Release);
    unsafe { write(state.mmio, RDT, head as u32) };
    if !end_of_packet {
        return None;
    }
    Some(length)
}
