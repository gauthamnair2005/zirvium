//! PCI bus enumeration and graphics device discovery.
//!
//! Zirvium does not rely on firmware device paths: after boot services exit we
//! own the machine, so we walk configuration space ourselves.  The graphics
//! device is reported to DisplayJet, which uses the firmware-programmed
//! scanout and a CPU SIMD rasteriser.

use alloc::vec::Vec;
use zirvstdlib::abi::{GpuDriver, GpuInfo, GpuVendor, set_name};
use zirvstdlib::sync::Once;

use crate::arch::port::{inl, outl};

const CONFIG_ADDRESS: u16 = 0xCF8;
const CONFIG_DATA: u16 = 0xCFC;

#[derive(Clone, Copy, Debug)]
pub struct PciDevice {
    pub bus: u8,
    pub slot: u8,
    pub function: u8,
    pub vendor: u16,
    pub device: u16,
    pub class: u8,
    pub subclass: u8,
    pub programming_interface: u8,
    pub revision: u8,
    pub header_type: u8,
    pub bar: [u32; 6],
}

impl PciDevice {
    pub fn is_display(&self) -> bool {
        self.class == 0x03
    }

    pub fn is_network(&self) -> bool {
        self.class == 0x02
    }

    pub fn enable_bus_mastering(&self) {
        let mut command = config_read(self.bus, self.slot, self.function, 0x04) & 0xffff;
        command |= 1 << 1;
        command |= 1 << 2;
        config_write(self.bus, self.slot, self.function, 0x04, command);
    }

    pub fn address(&self) -> u32 {
        ((self.bus as u32) << 16) | ((self.slot as u32) << 11) | ((self.function as u32) << 8)
    }

    /// The first memory BAR, if any.
    pub fn memory_bar(&self) -> Option<(u64, u32)> {
        for bar in self.bar {
            if bar != 0 && bar & 0x1 == 0 {
                let address = (bar & !0xF) as u64;
                // 64-bit BARs are encoded in a pair; the upper half follows.
                return Some((address, bar));
            }
        }
        None
    }
}

static DEVICES: Once<Vec<PciDevice>> = Once::new();
static GPU: Once<GpuInfo> = Once::new();

fn config_read(bus: u8, slot: u8, function: u8, offset: u8) -> u32 {
    let address = 0x8000_0000u32
        | ((bus as u32) << 16)
        | ((slot as u32) << 11)
        | ((function as u32) << 8)
        | (offset as u32 & 0xFC);
    // SAFETY: the PCI configuration ports are architectural.
    unsafe {
        outl(CONFIG_ADDRESS, address);
        inl(CONFIG_DATA)
    }
}

fn config_write(bus: u8, slot: u8, function: u8, offset: u8, value: u32) {
    let address = 0x8000_0000u32
        | ((bus as u32) << 16)
        | ((slot as u32) << 11)
        | ((function as u32) << 8)
        | (offset as u32 & 0xFC);
    // SAFETY: the PCI configuration ports are architectural.
    unsafe {
        outl(CONFIG_ADDRESS, address);
        outl(CONFIG_DATA, value);
    }
}

fn read_device(bus: u8, slot: u8, function: u8) -> Option<PciDevice> {
    let identity = config_read(bus, slot, function, 0x00);
    let vendor = (identity & 0xFFFF) as u16;
    if vendor == 0xFFFF {
        return None;
    }
    let device = (identity >> 16) as u16;
    let revision = (config_read(bus, slot, function, 0x08) & 0xFF) as u8;
    let class_word = config_read(bus, slot, function, 0x08);
    let class = ((class_word >> 24) & 0xFF) as u8;
    let subclass = ((class_word >> 16) & 0xFF) as u8;
    let programming_interface = ((class_word >> 8) & 0xFF) as u8;
    let header = config_read(bus, slot, function, 0x0C);
    let header_type = ((header >> 16) & 0xFF) as u8;
    let mut bar = [0u32; 6];
    if header_type & 0x7F == 0 {
        for (index, value) in bar.iter_mut().enumerate() {
            *value = config_read(bus, slot, function, 0x10 + index as u8 * 4);
        }
    }
    Some(PciDevice {
        bus,
        slot,
        function,
        vendor,
        device,
        class,
        subclass,
        programming_interface,
        revision,
        header_type,
        bar,
    })
}

fn enumerate() -> Vec<PciDevice> {
    let mut devices = Vec::new();
    // Bus 0 is guaranteed; scan a handful more for bridges on real machines.
    for bus in 0u16..=255 {
        for slot in 0u8..32 {
            let identity = config_read(bus as u8, slot, 0, 0);
            if identity & 0xFFFF == 0xFFFF {
                continue;
            }
            let header = config_read(bus as u8, slot, 0, 0x0C);
            let functions = if header >> 23 & 1 != 0 { 8 } else { 1 };
            for function in 0..functions {
                if let Some(device) = read_device(bus as u8, slot, function) {
                    devices.push(device);
                }
            }
        }
    }
    devices
}

fn vendor_name(vendor: u16) -> &'static str {
    match vendor {
        0x8086 => "Intel",
        0x1002 | 0x1022 => "AMD",
        0x10DE => "NVIDIA",
        0x1AF4 => "Virtio",
        0x1A03 => "ASPEED",
        0x1234 => "QEMU",
        0x15AD => "VMware",
        0x106B => "Apple",
        0x102B => "Matrox",
        _ => "Unknown",
    }
}

fn vendor_enum(vendor: u16) -> GpuVendor {
    match vendor {
        0x8086 => GpuVendor::Intel,
        0x1002 | 0x1022 => GpuVendor::Amd,
        0x10DE => GpuVendor::Nvidia,
        0x1AF4 => GpuVendor::Virtio,
        0x106B => GpuVendor::Apple,
        0xFFFF => GpuVendor::None,
        _ => GpuVendor::Other,
    }
}

fn gpu_name(device: &PciDevice) -> &'static str {
    match (device.vendor, device.device) {
        (0x1AF4, _) => "Virtio GPU (scanout)",
        (0x1234, 0x1111) => "QEMU Standard VGA",
        (0x15AD, _) => "VMware SVGA",
        (0x8086, _) => "Intel integrated graphics",
        (0x1002, _) => "AMD Radeon graphics",
        (0x10DE, _) => "NVIDIA graphics",
        (0x1A03, _) => "ASPEED BMC graphics",
        _ => "PCI display adapter",
    }
}

/// Probe a memory BAR's size by writing all ones and restoring it.
fn bar_size(bus: u8, slot: u8, function: u8, offset: u8) -> u32 {
    let original = config_read(bus, slot, function, offset);
    if original == 0 || original & 0x1 != 0 {
        return 0;
    }
    config_write(bus, slot, function, offset, 0xFFFF_FFF0);
    let mask = config_read(bus, slot, function, offset) & 0xFFFF_FFF0;
    config_write(bus, slot, function, offset, original);
    if mask == 0 {
        0
    } else {
        (!mask).wrapping_add(1)
    }
}

fn build_gpu_info(device: Option<&PciDevice>) -> GpuInfo {
    let mut info = GpuInfo {
        vendor: GpuVendor::None,
        driver: GpuDriver::FramebufferSimd,
        vendor_id: 0,
        device_id: 0,
        mmio_base: 0,
        mmio_size: 0,
        accelerated: false,
        _reserved: [0; 7],
        name: [0; 64],
    };
    let Some(device) = device else {
        set_name(&mut info.name, "Firmware framebuffer");
        return info;
    };

    info.vendor = vendor_enum(device.vendor);
    info.vendor_id = device.vendor;
    info.device_id = device.device;

    // Prefer a 64-bit memory BAR when present.
    let mut index = 0;
    while index < 6 {
        let bar = device.bar[index];
        if bar != 0 && bar & 0x1 == 0 {
            let mut base = (bar & !0xF) as u64;
            let mut size = bar_size(
                device.bus,
                device.slot,
                device.function,
                0x10 + index as u8 * 4,
            );
            let is_64 = (bar >> 1) & 0x3 == 0x2;
            if is_64 && index + 1 < 6 {
                let upper = device.bar[index + 1] as u64;
                base |= upper << 32;
                let upper_size = bar_size(
                    device.bus,
                    device.slot,
                    device.function,
                    0x10 + (index as u8 + 1) * 4,
                );
                size = size.max(upper_size);
            }
            info.mmio_base = base;
            info.mmio_size = size as u64;
            break;
        }
        index += if (bar >> 1) & 0x3 == 0x2 { 2 } else { 1 };
    }

    let _ = vendor_name(device.vendor);
    set_name(&mut info.name, gpu_name(device));
    info
}

/// Enumerate PCI and select a graphics device.
pub fn init() {
    let devices = DEVICES.call_once(enumerate);
    crate::sprintln!("PCI: {} devices discovered", devices.len());

    let mut candidate: Option<&PciDevice> = None;
    for device in devices.iter().filter(|device| device.is_display()) {
        crate::sprintln!(
            "PCI: {:02x}:{:02x}.{} {:04x}:{:04x} class {:02x}.{:02x} rev {:02x}",
            device.bus,
            device.slot,
            device.function,
            device.vendor,
            device.device,
            device.class,
            device.subclass,
            device.revision
        );
        // Prefer Intel integrated graphics, then virtio, then anything else.
        candidate = match candidate {
            None => Some(device),
            Some(current) => {
                let rank = |vendor: u16| match vendor {
                    0x8086 => 3u8,
                    0x1AF4 => 2,
                    _ => 1,
                };
                if rank(device.vendor) > rank(current.vendor) {
                    Some(device)
                } else {
                    Some(current)
                }
            }
        };
    }

    let info = build_gpu_info(candidate);
    crate::sprintln!(
        "PCI: GPU {} {:04x}:{:04x} mmio {:#x}+{:#x}",
        info.name_str(),
        info.vendor_id,
        info.device_id,
        info.mmio_base,
        info.mmio_size
    );
    GPU.call_once(|| info);
}

pub fn devices() -> &'static [PciDevice] {
    DEVICES
        .get()
        .map(|devices| devices.as_slice())
        .unwrap_or(&[])
}

pub fn gpu() -> GpuInfo {
    GPU.get().copied().unwrap_or(GpuInfo {
        vendor: GpuVendor::None,
        driver: GpuDriver::FramebufferSimd,
        vendor_id: 0,
        device_id: 0,
        mmio_base: 0,
        mmio_size: 0,
        accelerated: false,
        _reserved: [0; 7],
        name: [0; 64],
    })
}

/// Total number of discovered PCI functions.
pub fn device_count() -> usize {
    devices().len()
}
