//! Generic virtual hardware driver registry.
//!
//! This layer identifies safe, well-known virtual devices without claiming
//! that their command protocols are implemented.  It is the common discovery
//! surface for the display and network services.

use alloc::vec::Vec;

use zirvstdlib::sync::Once;

use super::pci::PciDevice;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DriverKind {
    VirtioGpu,
    VirtioNet,
    QemuVga,
    QemuE1000,
    VmwareSvga,
    VmwareNet,
    VirtualBoxVga,
    VirtualBoxNet,
    GenericDisplay,
    GenericNetwork,
}

#[derive(Clone, Copy, Debug)]
pub struct DriverInfo {
    pub kind: DriverKind,
    pub vendor: u16,
    pub device: u16,
    pub bus: u8,
    pub slot: u8,
    pub function: u8,
    pub ready: bool,
}

static DRIVERS: Once<Vec<DriverInfo>> = Once::new();

fn identify(device: &PciDevice) -> Option<DriverInfo> {
    let kind = match (device.vendor, device.device, device.class) {
        (0x1af4, 0x1050..=0x107f, 0x03) => DriverKind::VirtioGpu,
        (0x1af4, 0x1040..=0x104f, 0x02) => DriverKind::VirtioNet,
        (0x1234, 0x1111, 0x03) => DriverKind::QemuVga,
        (0x8086, 0x100e | 0x10d3 | 0x10d9, 0x02) => DriverKind::QemuE1000,
        (0x15ad, 0x0405, 0x03) => DriverKind::VmwareSvga,
        (0x15ad, 0x07b0, 0x02) => DriverKind::VmwareNet,
        (0x80ee, 0xbeef, 0x03) => DriverKind::VirtualBoxVga,
        (0x80ee, 0xcafe, 0x02) => DriverKind::VirtualBoxNet,
        _ if device.is_display() => DriverKind::GenericDisplay,
        _ if device.is_network() => DriverKind::GenericNetwork,
        _ => return None,
    };
    Some(DriverInfo {
        kind,
        vendor: device.vendor,
        device: device.device,
        bus: device.bus,
        slot: device.slot,
        function: device.function,
        // Protocol command queues/rings are deliberately not enabled yet.
        ready: false,
    })
}

pub fn init() {
    let mut found = Vec::new();
    for device in super::pci::devices() {
        if let Some(driver) = identify(device) {
            crate::sprintln!(
                "DRIVER: {:?} {:04x}:{:04x} at {:02x}:{:02x}.{} detected",
                driver.kind,
                driver.vendor,
                driver.device,
                driver.bus,
                driver.slot,
                driver.function
            );
            found.push(driver);
        }
    }
    DRIVERS.call_once(|| found);
}

pub fn all() -> &'static [DriverInfo] {
    DRIVERS.get().map(Vec::as_slice).unwrap_or(&[])
}

pub fn find(kind: DriverKind) -> Option<DriverInfo> {
    all().iter().copied().find(|driver| driver.kind == kind)
}

pub fn network_present() -> bool {
    all().iter().any(|driver| {
        matches!(
            driver.kind,
            DriverKind::VirtioNet
                | DriverKind::QemuE1000
                | DriverKind::VmwareNet
                | DriverKind::VirtualBoxNet
                | DriverKind::GenericNetwork
        )
    })
}
