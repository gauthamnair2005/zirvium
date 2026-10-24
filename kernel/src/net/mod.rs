//! Small network service boundary.
//!
//! The service provides loopback plus the currently supported virtual Ethernet
//! path. Higher-level tools use this boundary instead of bypassing driver state.

pub mod protocol;

use alloc::string::String;

use zirvstdlib::sync::Once;

use protocol::{Ipv4, Mac};

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
        ethernet: crate::drivers::e1000::state()
            .map(|state| state.ready && state.link_up)
            .unwrap_or(false),
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
        address: if crate::drivers::e1000::state()
            .map(|state| state.ready && state.link_up)
            .unwrap_or(false)
        {
            [10, 0, 2, 15]
        } else {
            [127, 0, 0, 1]
        },
        mac: crate::drivers::e1000::state().map(|state| state.mac).unwrap_or([0; 6]),
        link_up: crate::drivers::e1000::state()
            .map(|state| state.link_up)
            .unwrap_or(false),
    };
    STATUS.call_once(|| status);
    crate::sprintln!(
        "NET: loopback online, {} external driver(s) detected; ethernet={}",
        status.driver_count,
        status.ethernet
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
        "gateway" | "10.0.2.2" => [10, 0, 2, 2],
        _ => return Err("unknown host; supported names are localhost and gateway"),
    };
    if address != [127, 0, 0, 1] {
        let state = crate::drivers::e1000::state().ok_or("network device is unavailable")?;
        if !state.ready || !state.link_up {
            return Err("network link is down");
        }
        let source_ip = Ipv4([10, 0, 2, 15]);
        let gateway_ip = Ipv4(address);
        let arp = protocol::arp_request(Mac(state.mac), source_ip, gateway_ip);
        crate::drivers::e1000::transmit(&arp)?;
        let start = crate::arch::pit::uptime_ms();
        let mut receive_buffer = [0u8; 2048];
        let gateway_mac = loop {
            if crate::arch::pit::uptime_ms().saturating_sub(start) >= 1000 {
                return Err("ARP resolution timed out");
            }
            if let Some(length) = crate::drivers::e1000::receive(&mut receive_buffer)
                && let Some((header, payload)) =
                    protocol::parse_ethernet(&receive_buffer[..length])
                && header.ethertype == protocol::ETHERTYPE_ARP
                && let Some(mac) = protocol::parse_arp_reply(payload, source_ip)
            {
                break mac;
            }
        };
        let request = protocol::icmp_echo_request(0x5a52, sequence as u16, b"zirvium");
        let packet = protocol::ipv4_packet(
            source_ip,
            gateway_ip,
            &request,
        );
        let frame = protocol::ethernet_frame(
            gateway_mac,
            Mac(state.mac),
            protocol::ETHERTYPE_IPV4,
            &packet,
        );
        crate::drivers::e1000::transmit(&frame)?;
        let start = crate::arch::pit::uptime_ms();
        while crate::arch::pit::uptime_ms().saturating_sub(start) < 1000 {
            if let Some(length) = crate::drivers::e1000::receive(&mut receive_buffer)
                && let Some((header, payload)) = protocol::parse_ethernet(&receive_buffer[..length])
                && header.ethertype == protocol::ETHERTYPE_IPV4
                && let Some((source, _, reply_sequence)) =
                    protocol::parse_icmp_echo_reply(payload)
                && source.0 == address
                && reply_sequence == sequence as u16
            {
                return Ok(PingResult {
                    address,
                    sequence,
                    elapsed_ms: crate::arch::pit::uptime_ms().saturating_sub(start) as u32,
                });
            }
        }
        return Err("ping timed out");
    }
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
