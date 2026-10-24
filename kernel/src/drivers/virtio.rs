//! Virtio 1.x transport and split virtqueue primitives.
//!
//! These structures are deliberately transport-neutral.  They provide the
//! layout and feature/status vocabulary required by Virtio-GPU and Virtio-Net;
//! device activation remains behind the platform registry until PCI capability
//! ownership and DMA memory reservation are available.

use core::sync::atomic::{AtomicU16, Ordering};

pub const STATUS_ACKNOWLEDGE: u8 = 1;
pub const STATUS_DRIVER: u8 = 2;
pub const STATUS_DRIVER_OK: u8 = 4;
pub const STATUS_FEATURES_OK: u8 = 8;
pub const STATUS_FAILED: u8 = 128;

pub const FEATURE_VERSION_1: u64 = 1 << 32;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Descriptor {
    pub address: u64,
    pub length: u32,
    pub flags: u16,
    pub next: u16,
}

pub const DESC_NEXT: u16 = 1;
pub const DESC_WRITE: u16 = 2;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Available {
    pub flags: u16,
    pub index: u16,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UsedElement {
    pub id: u32,
    pub length: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Used {
    pub flags: u16,
    pub index: u16,
}

pub struct QueueState {
    pub size: u16,
    pub next: AtomicU16,
}

impl QueueState {
    pub const fn new(size: u16) -> Self {
        Self {
            size,
            next: AtomicU16::new(0),
        }
    }

    pub fn claim(&self) -> u16 {
        self.next.fetch_add(1, Ordering::AcqRel) % self.size.max(1)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Capability {
    CommonConfig,
    NotifyConfig,
    IsrConfig,
    DeviceConfig,
    Unknown(u8),
}

impl Capability {
    pub fn from_type(value: u8) -> Self {
        match value {
            1 => Self::CommonConfig,
            2 => Self::NotifyConfig,
            3 => Self::IsrConfig,
            4 => Self::DeviceConfig,
            other => Self::Unknown(other),
        }
    }
}

pub fn dma_barrier() {
    core::sync::atomic::fence(Ordering::SeqCst);
}
