#include "stack.h"
#include <stdint.h>
#include <string.h>

/* ── Packet header structures ────────────────────────────────────────────── */

typedef struct eth_hdr {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t type;
} __attribute__((packed)) eth_hdr_t;

#define ETH_TYPE_ARP  0x0608
#define ETH_TYPE_IP   0x0008

typedef struct arp_pkt {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t op;
    uint8_t  sender_mac[6];
    uint8_t  sender_ip[4];
    uint8_t  target_mac[6];
    uint8_t  target_ip[4];
} __attribute__((packed)) arp_pkt_t;

#define ARP_HTYPE_ETH  1
#define ARP_PTYPE_IP   0x0008
#define ARP_OP_REQUEST 0x0100
#define ARP_OP_REPLY   0x0200

typedef struct ip_hdr {
    uint8_t  ver_ihl;
    uint8_t  dscp_ecn;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed)) ip_hdr_t;

#define IP_PROTO_UDP 17

typedef struct udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
} __attribute__((packed)) udp_hdr_t;

/* ── Static state ────────────────────────────────────────────────────────── */

static uint8_t     g_our_mac[NET_MAC_ALEN];
static net_send_fn g_send_fn = NULL;
static net_poll_fn g_poll_fn = NULL;

/* ── Byte-order helpers (little-endian host) ─────────────────────────────── */

static inline uint16_t htons(uint16_t x) { return (x >> 8) | (x << 8); }
static inline uint32_t htonl(uint32_t x) {
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void net_stack_init(const uint8_t mac[NET_MAC_ALEN])
{
    memcpy(g_our_mac, mac, NET_MAC_ALEN);
}

void net_stack_set_send(net_send_fn fn)
{
    g_send_fn = fn;
}

void net_stack_set_poll(net_poll_fn fn)
{
    g_poll_fn = fn;
}

/* ── IP checksum ─────────────────────────────────────────────────────────── */

static uint16_t ip_checksum(const void *buf, size_t len)
{
    uint32_t sum = 0;
    const uint16_t *p = (const uint16_t *)buf;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* ── ARP handler ─────────────────────────────────────────────────────────── */

static int handle_arp(const eth_hdr_t *eth, uint16_t len)
{
    if (len < sizeof(eth_hdr_t) + sizeof(arp_pkt_t)) return 0;
    const arp_pkt_t *arp = (const arp_pkt_t *)(eth + 1);

    if (arp->hw_type != ARP_HTYPE_ETH)        return 0;
    if (arp->proto_type != ARP_PTYPE_IP)       return 0;
    if (arp->hw_len != NET_MAC_ALEN)           return 0;
    if (arp->proto_len != NET_IP_ALEN)         return 0;

    uint32_t target_ip = (uint32_t)arp->target_ip[0] << 24 |
                         (uint32_t)arp->target_ip[1] << 16 |
                         (uint32_t)arp->target_ip[2] << 8  |
                         (uint32_t)arp->target_ip[3];

    if (target_ip != NET_GUEST_IP) return 0;

    if (arp->op == ARP_OP_REQUEST) {
        if (!g_send_fn) return 1;

        uint8_t reply_buf[sizeof(eth_hdr_t) + sizeof(arp_pkt_t)];
        eth_hdr_t *rep_eth = (eth_hdr_t *)reply_buf;
        arp_pkt_t *rep_arp = (arp_pkt_t *)(rep_eth + 1);

        memcpy(rep_eth->dst, arp->sender_mac, NET_MAC_ALEN);
        memcpy(rep_eth->src, g_our_mac, NET_MAC_ALEN);
        rep_eth->type = ETH_TYPE_ARP;

        rep_arp->hw_type    = ARP_HTYPE_ETH;
        rep_arp->proto_type = ARP_PTYPE_IP;
        rep_arp->hw_len     = NET_MAC_ALEN;
        rep_arp->proto_len  = NET_IP_ALEN;
        rep_arp->op         = ARP_OP_REPLY;
        memcpy(rep_arp->sender_mac, g_our_mac, NET_MAC_ALEN);
        memcpy(rep_arp->sender_ip, arp->target_ip, NET_IP_ALEN);
        memcpy(rep_arp->target_mac, arp->sender_mac, NET_MAC_ALEN);
        memcpy(rep_arp->target_ip, arp->sender_ip, NET_IP_ALEN);

        g_send_fn(reply_buf, sizeof(reply_buf));
        return 1;
    }

    return 0;
}

/* ── Packet dispatch ─────────────────────────────────────────────────────── */

int net_stack_rx(const uint8_t *frame, uint16_t len)
{
    if (len < sizeof(eth_hdr_t)) return 0;
    const eth_hdr_t *eth = (const eth_hdr_t *)frame;

    switch (eth->type) {
    case ETH_TYPE_ARP:
        return handle_arp(eth, len);
    case ETH_TYPE_IP:
        return 0;
    default:
        return 0;
    }
}

/* ── ARP resolution ──────────────────────────────────────────────────────── */

/* Send an ARP request for target_ip and wait for a reply.
   Returns the sender MAC on success (6 bytes copied into out_mac), or -1. */
static int arp_resolve_kernel(uint32_t target_ip, uint8_t *out_mac)
{
    if (!g_send_fn || !g_poll_fn) return -1;

    uint8_t buf[sizeof(eth_hdr_t) + sizeof(arp_pkt_t)];
    eth_hdr_t *eth = (eth_hdr_t *)buf;
    arp_pkt_t *arp = (arp_pkt_t *)(eth + 1);

    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(eth->dst, bcast, 6);
    memcpy(eth->src, g_our_mac, 6);
    eth->type = ETH_TYPE_ARP;

    arp->hw_type    = 1;
    arp->proto_type = 0x0008;
    arp->hw_len     = 6;
    arp->proto_len  = 4;
    arp->op         = ARP_OP_REQUEST;
    memcpy(arp->sender_mac, g_our_mac, 6);
    arp->sender_ip[0] = (NET_GUEST_IP >> 24) & 0xFF;
    arp->sender_ip[1] = (NET_GUEST_IP >> 16) & 0xFF;
    arp->sender_ip[2] = (NET_GUEST_IP >> 8) & 0xFF;
    arp->sender_ip[3] = NET_GUEST_IP & 0xFF;
    memset(arp->target_mac, 0, 6);
    arp->target_ip[0] = (target_ip >> 24) & 0xFF;
    arp->target_ip[1] = (target_ip >> 16) & 0xFF;
    arp->target_ip[2] = (target_ip >> 8) & 0xFF;
    arp->target_ip[3] = target_ip & 0xFF;

    g_send_fn(buf, sizeof(buf));

    for (int i = 0; i < 5000; i++) {
        uint8_t rbuf[2048];
        int n = g_poll_fn(rbuf, sizeof(rbuf));
        if (n <= 0) { __asm__("pause"); continue; }
        if (net_stack_rx(rbuf, (uint16_t)n)) continue;

        if (n < (int)(sizeof(eth_hdr_t) + sizeof(arp_pkt_t))) continue;
        const eth_hdr_t *re = (const eth_hdr_t *)rbuf;
        if (re->type != ETH_TYPE_ARP) continue;

        const arp_pkt_t *ra = (const arp_pkt_t *)(re + 1);
        if (ra->op != ARP_OP_REPLY) continue;

        uint32_t resp_ip = (uint32_t)ra->sender_ip[0] << 24 |
                          (uint32_t)ra->sender_ip[1] << 16 |
                          (uint32_t)ra->sender_ip[2] << 8  |
                          (uint32_t)ra->sender_ip[3];
        if (resp_ip != target_ip) continue;

        memcpy(out_mac, ra->sender_mac, 6);
        return 0;
    }
    return -1;
}

/* ── DNS resolution ──────────────────────────────────────────────────────── */

/* Build a DNS query for the given domain in the buffer.
   Returns the total query length. */
static uint16_t build_dns_query(uint8_t *buf, uint16_t id, const char *domain)
{
    /* DNS header (12 bytes) */
    struct {
        uint16_t id;
        uint16_t flags;
        uint16_t qdcount;
        uint16_t ancount;
        uint16_t nscount;
        uint16_t arcount;
    } *hdr = (void *)buf;

    hdr->id      = htons(id);
    hdr->flags   = htons(0x0100); /* standard query, recursion desired */
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;

    /* Encode domain name as length-prefixed labels */
    uint8_t *qname = buf + 12;
    const char *p = domain;
    while (*p) {
        const char *dot = strchr(p, '.');
        size_t seglen = dot ? (size_t)(dot - p) : strlen(p);
        if (seglen > 63) return 0;
        *qname++ = (uint8_t)seglen;
        memcpy(qname, p, seglen);
        qname += seglen;
        p = dot ? dot + 1 : p + seglen;
    }
    *qname++ = 0; /* root label */

    /* QTYPE = A (1), QCLASS = IN (1) */
    qname[0] = 0; qname[1] = 1; /* A record */
    qname[2] = 0; qname[3] = 1; /* IN class */
    qname += 4;

    return (uint16_t)(qname - buf);
}

/* Parse a DNS response and return the first A record IP (host byte order),
   or 0 if not found. */
static uint32_t parse_dns_response(const uint8_t *buf, uint16_t len)
{
    if (len < 12) return 0;

    uint16_t qdcount = (uint16_t)(buf[4] << 8) | buf[5];
    uint16_t ancount = (uint16_t)(buf[6] << 8) | buf[7];
    if (ancount == 0) return 0;

    /* Skip header (12 bytes) */
    const uint8_t *p = buf + 12;
    uint16_t remaining = len - 12;

    /* Skip the question section */
    for (uint16_t i = 0; i < qdcount; i++) {
        /* Skip encoded domain name */
        while (remaining > 0 && *p != 0) {
            if (*p & 0xC0) { p += 2; remaining -= 2; break; } /* pointer */
            else { p += *p + 1; remaining -= *p + 1; }
        }
        if (remaining == 0) return 0;
        p++; remaining--; /* skip root label */
        if (remaining < 4) return 0;
        p += 4; remaining -= 4; /* skip QTYPE + QCLASS */
    }

    /* Parse answer section: find first A record */
    for (uint16_t i = 0; i < ancount; i++) {
        /* Skip name */
        while (remaining > 0 && *p != 0) {
            if (*p & 0xC0) { p += 2; remaining -= 2; break; }
            else { p += *p + 1; remaining -= *p + 1; }
        }
        if (remaining == 0) return 0;
        p++; remaining--; /* skip root label */

        if (remaining < 10) return 0;
        uint16_t type  = (uint16_t)(p[0] << 8) | p[1];
        uint16_t clazz = (uint16_t)(p[2] << 8) | p[3];
        p += 8; remaining -= 8; /* skip type, class, TTL */

        if (remaining < 2) return 0;
        uint16_t rdlength = (uint16_t)(p[0] << 8) | p[1];
        p += 2; remaining -= 2;

        if (type == 1 && clazz == 1 && rdlength == 4 && remaining >= 4) {
            /* A record — return the IP */
            return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                   ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
        }

        if (remaining < rdlength) return 0;
        p += rdlength;
        remaining -= rdlength;
    }

    return 0;
}

uint32_t net_stack_dns_resolve(const char *domain)
{
    if (!g_send_fn || !g_poll_fn) return 0;

    /* 1. ARP resolve the DNS server */
    uint8_t dns_mac[6];
    if (arp_resolve_kernel(NET_DNS_SERVER_IP, dns_mac) < 0)
        return 0;

    /* 2. Build DNS query */
    uint8_t dns_buf[512];
    uint16_t qlen = build_dns_query(dns_buf, 0x1234, domain);
    if (qlen == 0) return 0;

    /* 3. Build UDP datagram */
    uint8_t udp_buf[sizeof(udp_hdr_t) + 512];
    udp_hdr_t *udp = (udp_hdr_t *)udp_buf;
    uint16_t udp_len = sizeof(udp_hdr_t) + qlen;
    udp->src_port = htons(0xC000 + 0x1234);
    udp->dst_port = htons(53);
    udp->len      = htons(udp_len);
    udp->checksum = 0;
    memcpy(udp_buf + sizeof(udp_hdr_t), dns_buf, qlen);

    /* 4. Build IP datagram */
    uint8_t ip_buf[sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + 512];
    ip_hdr_t *ip = (ip_hdr_t *)ip_buf;
    uint16_t ip_total = sizeof(ip_hdr_t) + udp_len;
    memset(ip, 0, sizeof(ip_hdr_t));
    ip->ver_ihl    = 0x45;
    ip->total_len  = htons(ip_total);
    ip->id         = htons(0x0001);
    ip->ttl        = 64;
    ip->protocol   = IP_PROTO_UDP;
    ip->src_ip     = htonl(NET_GUEST_IP);
    ip->dst_ip     = htonl(NET_DNS_SERVER_IP);
    ip->checksum   = ip_checksum(ip, sizeof(ip_hdr_t));
    memcpy(ip_buf + sizeof(ip_hdr_t), udp_buf, udp_len);

    /* 5. Build Ethernet frame */
    uint8_t frame[sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + 512];
    eth_hdr_t *eth = (eth_hdr_t *)frame;
    uint16_t frame_len = sizeof(eth_hdr_t) + ip_total;
    memcpy(eth->dst, dns_mac, 6);
    memcpy(eth->src, g_our_mac, 6);
    eth->type = ETH_TYPE_IP;
    memcpy(frame + sizeof(eth_hdr_t), ip_buf, ip_total);

    /* 6. Send */
    g_send_fn(frame, frame_len);

    /* 7. Poll for DNS response (try up to ~64000 polls) */
    for (int i = 0; i < 64000; i++) {
        uint8_t rbuf[2048];
        int n = g_poll_fn(rbuf, sizeof(rbuf));
        if (n <= 0) {
            if (i % 64 == 0) __asm__("pause");
            continue;
        }
        if (net_stack_rx(rbuf, (uint16_t)n)) continue;
        if (n < (int)(sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(udp_hdr_t)))
            continue;

        const eth_hdr_t *re = (const eth_hdr_t *)rbuf;
        if (re->type != ETH_TYPE_IP) continue;

        const ip_hdr_t *ri = (const ip_hdr_t *)(re + 1);
        if (ri->protocol != IP_PROTO_UDP) continue;

        /* Verify it's from the DNS server (network byte order) */
        if (ri->src_ip != htonl(NET_DNS_SERVER_IP)) continue;

        const udp_hdr_t *rudp = (const udp_hdr_t *)(ri + 1);
        if (htons(rudp->dst_port) != 0xC000 + 0x1234) continue;

        uint16_t dns_resp_len = htons(rudp->len) - sizeof(udp_hdr_t);
        const uint8_t *dns_resp = (const uint8_t *)(rudp + 1);

        uint32_t ip_result = parse_dns_response(dns_resp, dns_resp_len);
        if (ip_result != 0)
            return ip_result;
    }

    return 0;
}
