//! Ethernet, ARP, IPv4 and ICMP packet primitives.

use alloc::vec::Vec;

pub const ETHERTYPE_IPV4: u16 = 0x0800;
pub const ETHERTYPE_ARP: u16 = 0x0806;
pub const IPPROTO_ICMP: u8 = 1;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Mac(pub [u8; 6]);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Ipv4(pub [u8; 4]);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EthernetHeader {
    pub destination: Mac,
    pub source: Mac,
    pub ethertype: u16,
}

pub fn parse_ethernet(frame: &[u8]) -> Option<(EthernetHeader, &[u8])> {
    if frame.len() < 14 {
        return None;
    }
    Some((
        EthernetHeader {
            destination: Mac(frame[0..6].try_into().ok()?),
            source: Mac(frame[6..12].try_into().ok()?),
            ethertype: u16::from_be_bytes([frame[12], frame[13]]),
        },
        &frame[14..],
    ))
}

pub fn parse_icmp_echo_reply(packet: &[u8]) -> Option<(Ipv4, u16, u16)> {
    if packet.len() < 28 || packet[0] >> 4 != 4 || packet[9] != IPPROTO_ICMP {
        return None;
    }
    let header_len = ((packet[0] & 0x0f) * 4) as usize;
    if packet.len() < header_len + 8 || packet[header_len] != 0 {
        return None;
    }
    Some((
        Ipv4(packet[12..16].try_into().ok()?),
        u16::from_be_bytes([packet[header_len + 4], packet[header_len + 5]]),
        u16::from_be_bytes([packet[header_len + 6], packet[header_len + 7]]),
    ))
}

pub fn checksum(bytes: &[u8]) -> u16 {
    let mut sum = 0u32;
    let mut index = 0;
    while index + 1 < bytes.len() {
        sum += u16::from_be_bytes([bytes[index], bytes[index + 1]]) as u32;
        index += 2;
    }
    if index < bytes.len() {
        sum += (bytes[index] as u32) << 8;
    }
    while sum >> 16 != 0 {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    !(sum as u16)
}

pub fn ethernet_frame(destination: Mac, source: Mac, ethertype: u16, payload: &[u8]) -> Vec<u8> {
    let mut frame = Vec::with_capacity(14 + payload.len());
    frame.extend_from_slice(&destination.0);
    frame.extend_from_slice(&source.0);
    frame.extend_from_slice(&ethertype.to_be_bytes());
    frame.extend_from_slice(payload);
    frame
}

pub fn arp_request(source_mac: Mac, source_ip: Ipv4, target_ip: Ipv4) -> Vec<u8> {
    let mut arp = Vec::with_capacity(28);
    arp.extend_from_slice(&1u16.to_be_bytes());
    arp.extend_from_slice(&ETHERTYPE_IPV4.to_be_bytes());
    arp.push(6);
    arp.push(4);
    arp.extend_from_slice(&1u16.to_be_bytes());
    arp.extend_from_slice(&source_mac.0);
    arp.extend_from_slice(&source_ip.0);
    arp.extend_from_slice(&[0; 6]);
    arp.extend_from_slice(&target_ip.0);
    ethernet_frame(Mac([0xff; 6]), source_mac, ETHERTYPE_ARP, &arp)
}

pub fn parse_arp_reply(payload: &[u8], target_ip: Ipv4) -> Option<Mac> {
    if payload.len() < 28
        || u16::from_be_bytes([payload[0], payload[1]]) != 1
        || u16::from_be_bytes([payload[2], payload[3]]) != ETHERTYPE_IPV4
        || u16::from_be_bytes([payload[6], payload[7]]) != 2
        || payload[24..28] != target_ip.0
    {
        return None;
    }
    Some(Mac(payload[8..14].try_into().ok()?))
}

pub fn icmp_echo_request(identifier: u16, sequence: u16, payload: &[u8]) -> Vec<u8> {
    let mut packet = Vec::with_capacity(8 + payload.len());
    packet.extend_from_slice(&[8, 0, 0, 0]);
    packet.extend_from_slice(&identifier.to_be_bytes());
    packet.extend_from_slice(&sequence.to_be_bytes());
    packet.extend_from_slice(payload);
    let sum = checksum(&packet);
    packet[2..4].copy_from_slice(&sum.to_be_bytes());
    packet
}

pub fn ipv4_packet(source: Ipv4, destination: Ipv4, payload: &[u8]) -> Vec<u8> {
    let total = 20 + payload.len();
    let mut packet = Vec::with_capacity(total);
    packet.extend_from_slice(&[0x45, 0]);
    packet.extend_from_slice(&(total as u16).to_be_bytes());
    packet.extend_from_slice(&[0, 0, 0, 0]);
    packet.push(64);
    packet.push(IPPROTO_ICMP);
    packet.extend_from_slice(&[0, 0]);
    packet.extend_from_slice(&source.0);
    packet.extend_from_slice(&destination.0);
    let sum = checksum(&packet[..20]);
    packet[10..12].copy_from_slice(&sum.to_be_bytes());
    packet.extend_from_slice(payload);
    packet
}
