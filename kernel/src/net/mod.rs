//! Small network service boundary.
//!
//! The current kernel has no NIC driver yet, so the service provides a real
//! loopback interface and explicit capability reporting.  Higher-level tools
//! use this boundary instead of pretending that an unavailable Ethernet device
//! is online.

pub mod protocol;

use alloc::string::String;

use zirvstdlib::sync::Once;

#[derive(Clone, Copy)]
pub struct NetworkStatus {
    pub loopback: bool,
    pub ethernet: bool,
    pub driver_count: usize,
    pub address: [u8; 4],
    pub mac: [u8; 6],
    pub link_up: bool,
}

static STATUS: Once<NetworkStatus> = Once::new();

pub fn init() {
    let status = NetworkStatus {
        loopback: true,
        // Detection is exposed separately from readiness: no packet-ring
        // protocol has been enabled yet.
        ethernet: false,
        driver_count: crate::drivers::platform::all()
            .iter()
            .filter(|driver| {
                matches!(
                    driver.kind,
                    crate::drivers::platform::DriverKind::VirtioNet
                        | crate::drivers::platform::DriverKind::QemuE1000
                        | crate::drivers::platform::DriverKind::VmwareNet
                        | crate::drivers::platform::DriverKind::VirtualBoxNet
                        | crate::drivers::platform::DriverKind::GenericNetwork
                )
            })
            .count(),
        address: [127, 0, 0, 1],
        mac: crate::drivers::e1000::state().map(|state| state.mac).unwrap_or([0; 6]),
        link_up: crate::drivers::e1000::state()
            .map(|state| state.link_up)
            .unwrap_or(false),
    };
    STATUS.call_once(|| status);
    crate::sprintln!(
        "NET: loopback online, {} external driver(s) detected; protocol bring-up pending",
        status.driver_count
    );
}

pub fn status() -> NetworkStatus {
    STATUS.get().copied().unwrap_or(NetworkStatus {
        loopback: false,
        ethernet: false,
        driver_count: 0,
        address: [0, 0, 0, 0],
        mac: [0; 6],
        link_up: false,
    })
}

pub struct PingResult {
    pub address: [u8; 4],
    pub sequence: u32,
    pub elapsed_ms: u32,
}

pub fn ping(host: &str, sequence: u32) -> Result<PingResult, &'static str> {
    let address = match host {
        "localhost" | "127.0.0.1" => [127, 0, 0, 1],
        _ => return Err("host is unreachable: no network interface is configured"),
    };
    Ok(PingResult {
        address,
        sequence,
        elapsed_ms: 0,
    })
}

pub fn address_string(address: [u8; 4]) -> String {
    alloc::format!("{}.{}.{}.{}", address[0], address[1], address[2], address[3])
}

pub fn http_available() -> bool {
    false
}
