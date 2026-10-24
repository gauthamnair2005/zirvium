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

fn parse_ipv4(host: &str) -> Option<[u8; 4]> {
    let mut parts = [0u8; 4];
    let mut count = 0;
    for part in host.split('.') {
        if count >= 4 || part.is_empty() {
            return None;
        }
        parts[count] = part.parse().ok()?;
        count += 1;
    }
    (count == 4).then_some(parts)
}

fn resolve(host: &str) -> Result<[u8; 4], &'static str> {
    if host.is_empty() || host.len() > 253 {
        return Err("invalid hostname");
    }
    let state = crate::drivers::e1000::state().ok_or("network device is unavailable")?;
    if !state.ready || !state.link_up {
        return Err("network link is down");
    }
    let mut query = alloc::vec::Vec::new();
    query.extend_from_slice(&0x5a52u16.to_be_bytes());
    query.extend_from_slice(&0x0100u16.to_be_bytes());
    query.extend_from_slice(&1u16.to_be_bytes());
    query.extend_from_slice(&[0, 0, 0, 0]);
    for label in host.split('.') {
        if label.is_empty() || label.len() > 63 {
            return Err("invalid hostname");
        }
        query.push(label.len() as u8);
        query.extend_from_slice(label.as_bytes());
    }
    query.push(0);
    query.extend_from_slice(&1u16.to_be_bytes());
    query.extend_from_slice(&1u16.to_be_bytes());
    let source = Ipv4([10, 0, 2, 15]);
    let dns = Ipv4([10, 0, 2, 3]);
    let gateway = resolve_gateway(state, source)?;
    let udp = protocol::udp_packet(49152, 53, &query);
    let packet = protocol::ipv4_packet_with_protocol(source, dns, protocol::IPPROTO_UDP, &udp);
    let frame = protocol::ethernet_frame(gateway, Mac(state.mac), protocol::ETHERTYPE_IPV4, &packet);
    crate::drivers::e1000::transmit(&frame)?;
    let start = crate::arch::pit::uptime_ms();
    let mut buffer = [0u8; 2048];
    while crate::arch::pit::uptime_ms().saturating_sub(start) < 1500 {
        if let Some(length) = crate::drivers::e1000::receive(&mut buffer)
            && let Some((_, destination, source_port, _, payload)) =
                protocol::parse_udp(&buffer[..length])
            && destination.0 == source.0
            && source_port == 53
            && payload.len() >= 12
            && u16::from_be_bytes([payload[0], payload[1]]) == 0x5a52
            && payload[3] & 0x0f == 0
        {
            let answers = u16::from_be_bytes([payload[6], payload[7]]);
            let mut offset = 12;
            while offset < payload.len() && payload[offset] != 0 {
                offset += 1 + payload[offset] as usize;
            }
            offset += 5;
            for _ in 0..answers {
                if offset + 12 > payload.len() {
                    break;
                }
                offset += if payload[offset] & 0xc0 == 0xc0 {
                    2
                } else {
                    while offset < payload.len() && payload[offset] != 0 {
                        offset += 1 + payload[offset] as usize;
                    }
                    1
                };
                if offset + 10 > payload.len() {
                    break;
                }
                let record_type = u16::from_be_bytes([payload[offset], payload[offset + 1]]);
                let class = u16::from_be_bytes([payload[offset + 2], payload[offset + 3]]);
                let length = u16::from_be_bytes([payload[offset + 8], payload[offset + 9]]) as usize;
                offset += 10;
                if record_type == 1 && class == 1 && length == 4 && offset + 4 <= payload.len() {
                    return Ok(payload[offset..offset + 4].try_into().unwrap_or([0; 4]));
                }
                offset += length;
            }
        }
    }
    Err("DNS query timed out")
}

fn resolve_gateway(state: crate::drivers::e1000::E1000State, source: Ipv4) -> Result<Mac, &'static str> {
    let gateway = Ipv4([10, 0, 2, 2]);
    crate::drivers::e1000::transmit(&protocol::arp_request(Mac(state.mac), source, gateway))?;
    let start = crate::arch::pit::uptime_ms();
    let mut buffer = [0u8; 2048];
    while crate::arch::pit::uptime_ms().saturating_sub(start) < 1000 {
        if let Some(length) = crate::drivers::e1000::receive(&mut buffer)
            && let Some((header, payload)) = protocol::parse_ethernet(&buffer[..length])
            && header.ethertype == protocol::ETHERTYPE_ARP
            && let Some(mac) = protocol::parse_arp_reply(payload, source)
        {
            return Ok(mac);
        }
    }
    Err("ARP resolution timed out")
}

pub fn ping(host: &str, sequence: u32) -> Result<PingResult, &'static str> {
    let address = parse_ipv4(host)
        .or_else(|| resolve(host).ok())
        .ok_or("could not resolve host")?;
    if address != [127, 0, 0, 1] {
        let state = crate::drivers::e1000::state().ok_or("network device is unavailable")?;
        if !state.ready || !state.link_up {
            return Err("network link is down");
        }

        fn parse_ipv4(host: &str) -> Option<[u8; 4]> {
            let mut parts = [0u8; 4];
            let mut count = 0;
            for part in host.split('.') {
                if count >= 4 || part.is_empty() {
                    return None;
                }
                parts[count] = part.parse().ok()?;
                count += 1;
            }
            (count == 4).then_some(parts)
        }

        fn resolve(host: &str) -> Result<[u8; 4], &'static str> {
            if host.is_empty() || host.len() > 253 {
                return Err("invalid hostname");
            }
            let state = crate::drivers::e1000::state().ok_or("network device is unavailable")?;
            if !state.ready || !state.link_up {
                return Err("network link is down");
            }
            let mut query = alloc::vec::Vec::new();
            query.extend_from_slice(&0x5a52u16.to_be_bytes());
            query.extend_from_slice(&0x0100u16.to_be_bytes());
            query.extend_from_slice(&1u16.to_be_bytes());
            query.extend_from_slice(&[0, 0, 0, 0]);
            for label in host.split('.') {
                if label.is_empty() || label.len() > 63 {
                    return Err("invalid hostname");
                }
                query.push(label.len() as u8);
                query.extend_from_slice(label.as_bytes());
            }
            query.push(0);
            query.extend_from_slice(&1u16.to_be_bytes());
            query.extend_from_slice(&1u16.to_be_bytes());
            let source = Ipv4([10, 0, 2, 15]);
            let dns = Ipv4([10, 0, 2, 3]);
            let gateway = resolve_gateway(state, source)?;
            let udp = protocol::udp_packet(49152, 53, &query);
            let packet = protocol::ipv4_packet_with_protocol(source, dns, protocol::IPPROTO_UDP, &udp);
            let frame = protocol::ethernet_frame(gateway, Mac(state.mac), protocol::ETHERTYPE_IPV4, &packet);
            crate::drivers::e1000::transmit(&frame)?;
            let start = crate::arch::pit::uptime_ms();
            let mut buffer = [0u8; 2048];
            while crate::arch::pit::uptime_ms().saturating_sub(start) < 1500 {
                if let Some(length) = crate::drivers::e1000::receive(&mut buffer)
                    && let Some((_, destination, source_port, _, payload)) =
                        protocol::parse_udp(&buffer[..length])
                    && destination.0 == source.0
                    && source_port == 53
                    && payload.len() >= 12
                    && u16::from_be_bytes([payload[0], payload[1]]) == 0x5a52
                    && payload[3] & 0x0f == 0
                {
                    let answers = u16::from_be_bytes([payload[6], payload[7]]);
                    let mut offset = 12;
                    while offset < payload.len() && payload[offset] != 0 {
                        offset += 1 + payload[offset] as usize;
                    }
                    offset += 5;
                    for _ in 0..answers {
                        if offset + 12 > payload.len() {
                            break;
                        }
                        if payload[offset] & 0xc0 == 0xc0 {
                            offset += 2;
                        } else {
                            while offset < payload.len() && payload[offset] != 0 {
                                offset += 1 + payload[offset] as usize;
                            }
                            offset += 1;
                        }
                        if offset + 10 > payload.len() {
                            break;
                        }
                        let record_type = u16::from_be_bytes([payload[offset], payload[offset + 1]]);
                        let class = u16::from_be_bytes([payload[offset + 2], payload[offset + 3]]);
                        let length = u16::from_be_bytes([payload[offset + 8], payload[offset + 9]]) as usize;
                        offset += 10;
                        if record_type == 1 && class == 1 && length == 4 && offset + 4 <= payload.len() {
                            return Ok(payload[offset..offset + 4].try_into().unwrap_or([0; 4]));
                        }
                        offset += length;
                    }
                }
            }
            Err("DNS query timed out")
        }

        fn resolve_gateway(state: crate::drivers::e1000::E1000State, source: Ipv4) -> Result<Mac, &'static str> {
            let gateway = Ipv4([10, 0, 2, 2]);
            crate::drivers::e1000::transmit(&protocol::arp_request(Mac(state.mac), source, gateway))?;
            let start = crate::arch::pit::uptime_ms();
            let mut buffer = [0u8; 2048];
            while crate::arch::pit::uptime_ms().saturating_sub(start) < 1000 {
                if let Some(length) = crate::drivers::e1000::receive(&mut buffer)
                    && let Some((header, payload)) = protocol::parse_ethernet(&buffer[..length])
                    && header.ethertype == protocol::ETHERTYPE_ARP
                    && let Some(mac) = protocol::parse_arp_reply(payload, source)
                {
                    return Ok(mac);
                }
            }
            Err("ARP resolution timed out")
        }
        let source_ip = Ipv4([10, 0, 2, 15]);
        let gateway_ip = Ipv4([10, 0, 2, 2]);
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
            Ipv4(address),
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
