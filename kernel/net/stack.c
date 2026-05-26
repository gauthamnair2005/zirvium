#include "stack.h"
#include <stdint.h>
#include <string.h>
#include "kernel/console.h"
#include "kernel/time/time.h"
#include "kernel/proc/scheduler.h"

/* ── Mutable network config (defaults = QEMU SLiRP) ──────────────────────── */
uint32_t g_net_if_ip      = NET_GUEST_IP_DEFAULT;
uint32_t g_net_gateway    = NET_GATEWAY_IP_DEFAULT;
uint32_t g_net_dns_server = NET_DNS_SERVER_IP_DEFAULT;

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

    if (target_ip != g_net_if_ip) return 0;

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
    if (!g_send_fn || !g_poll_fn) { klog(LOG_FAIL, "ARP", "no send/poll fn"); return -1; }
    klog(LOG_INFO, "ARP", "resolving %d.%d.%d.%d",
         (target_ip >> 24) & 0xFF, (target_ip >> 16) & 0xFF,
         (target_ip >> 8) & 0xFF, target_ip & 0xFF);

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
    arp->sender_ip[0] = (g_net_if_ip >> 24) & 0xFF;
    arp->sender_ip[1] = (g_net_if_ip >> 16) & 0xFF;
    arp->sender_ip[2] = (g_net_if_ip >> 8) & 0xFF;
    arp->sender_ip[3] = g_net_if_ip & 0xFF;
    memset(arp->target_mac, 0, 6);
    arp->target_ip[0] = (target_ip >> 24) & 0xFF;
    arp->target_ip[1] = (target_ip >> 16) & 0xFF;
    arp->target_ip[2] = (target_ip >> 8) & 0xFF;
    arp->target_ip[3] = target_ip & 0xFF;

    g_send_fn(buf, sizeof(buf));
    time_msleep(1);

    uint64_t deadline = time_uptime_seconds() + 2;
    for (;;) {
        if (time_uptime_seconds() >= deadline) break;
        uint8_t rbuf[2048];
        int n = g_poll_fn(rbuf, sizeof(rbuf));
        if (n <= 0) { sched_yield(); continue; }
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
        klog(LOG_OK, "ARP", "resolved %d.%d.%d.%d ok",
             (target_ip >> 24) & 0xFF, (target_ip >> 16) & 0xFF,
             (target_ip >> 8) & 0xFF, target_ip & 0xFF);
        return 0;
    }
    klog(LOG_WARN, "ARP", "timeout for %d.%d.%d.%d",
         (target_ip >> 24) & 0xFF, (target_ip >> 16) & 0xFF,
         (target_ip >> 8) & 0xFF, target_ip & 0xFF);
    return -1;
}

/* ── Route resolve (ARP with gateway fallback) ────────────────────────────── */

/* Check if IP is on the local /24 subnet */
static inline int is_local_subnet(uint32_t ip)
{
    uint32_t mask = 0xFFFFFF00;
    return (ip & mask) == (g_net_if_ip & mask);
}

int net_stack_route_resolve(uint32_t target_ip, uint8_t *out_mac)
{
    uint32_t resolve_ip = target_ip;
    if (!is_local_subnet(target_ip)) {
        resolve_ip = g_net_gateway;
        klog(LOG_INFO, "ROUTE", "target external, routing via gateway %d.%d.%d.%d",
             (resolve_ip >> 24) & 0xFF, (resolve_ip >> 16) & 0xFF,
             (resolve_ip >> 8) & 0xFF, resolve_ip & 0xFF);
    }
    return arp_resolve_kernel(resolve_ip, out_mac);
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
        /* Skip name (may be a pointer 0xC0xx or label sequence) */
        int was_ptr = 0;
        while (remaining > 0 && *p != 0) {
            if (*p & 0xC0) { p += 2; remaining -= 2; was_ptr = 1; break; }
            else { p += *p + 1; remaining -= *p + 1; }
        }
        if (remaining == 0) return 0;
        if (!was_ptr) { p++; remaining--; } /* skip root label only for literal names */

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
    if (!g_send_fn || !g_poll_fn) { klog(LOG_FAIL, "DNS", "no send/poll fn"); return 0; }

    /* Try DNS server candidates: primary DNS, then gateway as fallback.
       QEMU SLiRP sometimes serves DNS at the gateway IP (10.0.2.2). */
    uint32_t dns_candidates[] = { g_net_dns_server, g_net_gateway };
    for (int ci = 0; ci < 2; ci++) {
        uint32_t dns_ip = dns_candidates[ci];
        if (dns_ip == 0) continue;

        klog(LOG_INFO, "DNS", "trying DNS server %d.%d.%d.%d",
             (dns_ip >> 24) & 0xFF, (dns_ip >> 16) & 0xFF,
             (dns_ip >> 8) & 0xFF, dns_ip & 0xFF);
        uint8_t dns_mac[6];
        if (arp_resolve_kernel(dns_ip, dns_mac) < 0) {
            klog(LOG_WARN, "DNS", "ARP failed for DNS server");
            continue;
        }
        uint8_t dns_buf[512];
        uint16_t qlen = build_dns_query(dns_buf, 0x1234, domain);
        if (qlen == 0) return 0;

        uint8_t udp_buf[sizeof(udp_hdr_t) + 512];
        udp_hdr_t *udp = (udp_hdr_t *)udp_buf;
        uint16_t udp_len = sizeof(udp_hdr_t) + qlen;
        udp->src_port = htons(0xC000 + 0x1234);
        udp->dst_port = htons(53);
        udp->len      = htons(udp_len);
        udp->checksum = 0;
        memcpy(udp_buf + sizeof(udp_hdr_t), dns_buf, qlen);

        uint8_t ip_buf[sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + 512];
        ip_hdr_t *ip = (ip_hdr_t *)ip_buf;
        uint16_t ip_total = sizeof(ip_hdr_t) + udp_len;
        memset(ip, 0, sizeof(ip_hdr_t));
        ip->ver_ihl    = 0x45;
        ip->total_len  = htons(ip_total);
        ip->id         = htons(0x0001);
        ip->ttl        = 64;
        ip->protocol   = IP_PROTO_UDP;
        ip->src_ip     = htonl(g_net_if_ip);
        ip->dst_ip     = htonl(dns_ip);
        ip->checksum   = ip_checksum(ip, sizeof(ip_hdr_t));
        memcpy(ip_buf + sizeof(ip_hdr_t), udp_buf, udp_len);

        uint8_t frame[sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + 512];
        eth_hdr_t *eth = (eth_hdr_t *)frame;
        uint16_t frame_len = sizeof(eth_hdr_t) + ip_total;
        memcpy(eth->dst, dns_mac, 6);
        memcpy(eth->src, g_our_mac, 6);
        eth->type = ETH_TYPE_IP;
        memcpy(frame + sizeof(eth_hdr_t), ip_buf, ip_total);

        g_send_fn(frame, frame_len);
        time_msleep(1);

        uint64_t deadline = time_uptime_seconds() + 3;
        for (;;) {
            if (time_uptime_seconds() >= deadline) break;
            uint8_t rbuf[2048];
            int n = g_poll_fn(rbuf, sizeof(rbuf));
            if (n <= 0) { sched_yield(); continue; }
            if (net_stack_rx(rbuf, (uint16_t)n)) continue;
            if (n < (int)(sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(udp_hdr_t)))
                continue;

            const eth_hdr_t *re = (const eth_hdr_t *)rbuf;
            if (re->type != ETH_TYPE_IP) continue;

            const ip_hdr_t *ri = (const ip_hdr_t *)(re + 1);
            if (ri->protocol != IP_PROTO_UDP) continue;

            if (ri->src_ip != htonl(dns_ip)) continue;

            const udp_hdr_t *rudp = (const udp_hdr_t *)(ri + 1);
            if (htons(rudp->dst_port) != 0xC000 + 0x1234) continue;

            uint16_t dns_resp_len = htons(rudp->len) - sizeof(udp_hdr_t);
            const uint8_t *dns_resp = (const uint8_t *)(rudp + 1);

            uint32_t ip_result = parse_dns_response(dns_resp, dns_resp_len);
            if (ip_result != 0) {
                klog(LOG_OK, "DNS", "resolved %s to %d.%d.%d.%d",
                     domain,
                     (ip_result >> 24) & 0xFF, (ip_result >> 16) & 0xFF,
                     (ip_result >> 8) & 0xFF, ip_result & 0xFF);
                return ip_result;
            }
        }
        klog(LOG_WARN, "DNS", "timeout for candidate %d.%d.%d.%d",
             (dns_ip >> 24) & 0xFF, (dns_ip >> 16) & 0xFF,
             (dns_ip >> 8) & 0xFF, dns_ip & 0xFF);
    }

    klog(LOG_FAIL, "DNS", "all candidates failed for '%s'", domain);
    return 0;
}

/* ── DHCP client (boot-time only) ─────────────────────────────────────────── */

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCP_MAGIC       0x63825363

/* DHCP message types (option 53) */
#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_ACK      5

/* DHCP option tags */
#define DHCP_OPT_PAD       0
#define DHCP_OPT_MSG_TYPE  53
#define DHCP_OPT_REQ_IP    50
#define DHCP_OPT_SRV_ID    54
#define DHCP_OPT_PARAM_REQ 55
#define DHCP_OPT_SUBNET    1
#define DHCP_OPT_ROUTER    3
#define DHCP_OPT_DNS       6
#define DHCP_OPT_END       255

/* DHCP message (BOOTP format) */
typedef struct __attribute__((packed)) dhcp_msg {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;
    uint8_t  options[312];
} dhcp_msg_t;

static uint8_t *dhcp_opt_byte(uint8_t *p, uint8_t tag, uint8_t val)
{
    *p++ = tag; *p++ = 1; *p++ = val;
    return p;
}

static uint8_t *dhcp_opt_data(uint8_t *p, uint8_t tag, const void *data, uint8_t len)
{
    *p++ = tag; *p++ = len; memcpy(p, data, len); return p + len;
}

static uint8_t *dhcp_opt_end(uint8_t *p)
{
    *p++ = DHCP_OPT_END; return p;
}

static uint16_t build_dhcp_discover(uint8_t *frame, uint32_t xid)
{
    uint8_t bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    eth_hdr_t *eth = (eth_hdr_t *)frame;
    memcpy(eth->dst, bcast_mac, 6);
    memcpy(eth->src, g_our_mac, 6);
    eth->type = ETH_TYPE_IP;
    ip_hdr_t *ip = (ip_hdr_t *)(eth + 1);
    memset(ip, 0, sizeof(ip_hdr_t));
    ip->ver_ihl   = 0x45;
    ip->total_len = 0;
    ip->id        = htons(0x0002);
    ip->ttl       = 64;
    ip->protocol  = IP_PROTO_UDP;
    ip->src_ip    = 0;
    ip->dst_ip    = htonl(0xFFFFFFFF);
    udp_hdr_t *udp = (udp_hdr_t *)(ip + 1);
    udp->src_port = htons(DHCP_CLIENT_PORT);
    udp->dst_port = htons(DHCP_SERVER_PORT);
    udp->checksum = 0;
    dhcp_msg_t *dhcp = (dhcp_msg_t *)(udp + 1);
    memset(dhcp, 0, sizeof(dhcp_msg_t));
    dhcp->op      = 1;
    dhcp->htype   = 1;
    dhcp->hlen    = 6;
    dhcp->xid     = xid;
    dhcp->flags   = htons(0x8000);
    memcpy(dhcp->chaddr, g_our_mac, 6);
    dhcp->magic   = htonl(DHCP_MAGIC);
    uint8_t *opt = dhcp->options;
    opt = dhcp_opt_byte(opt, DHCP_OPT_MSG_TYPE, DHCP_DISCOVER);
    uint8_t req_params[] = {DHCP_OPT_SUBNET, DHCP_OPT_ROUTER, DHCP_OPT_DNS};
    opt = dhcp_opt_data(opt, DHCP_OPT_PARAM_REQ, req_params, 3);
    opt = dhcp_opt_end(opt);
    uint16_t dhcp_len = (uint16_t)(uintptr_t)(opt - (uint8_t *)dhcp);
    uint16_t udp_len  = sizeof(udp_hdr_t) + dhcp_len;
    uint16_t ip_total = sizeof(ip_hdr_t) + udp_len;
    udp->len = htons(udp_len);
    ip->total_len = htons(ip_total);
    ip->checksum = ip_checksum(ip, sizeof(ip_hdr_t));
    return sizeof(eth_hdr_t) + ip_total;
}

static uint16_t build_dhcp_request(uint8_t *frame, uint32_t xid,
                                    uint32_t offered_ip, uint32_t server_ip)
{
    uint8_t bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    eth_hdr_t *eth = (eth_hdr_t *)frame;
    memcpy(eth->dst, bcast_mac, 6);
    memcpy(eth->src, g_our_mac, 6);
    eth->type = ETH_TYPE_IP;
    ip_hdr_t *ip = (ip_hdr_t *)(eth + 1);
    memset(ip, 0, sizeof(ip_hdr_t));
    ip->ver_ihl   = 0x45;
    ip->total_len = 0;
    ip->id        = htons(0x0003);
    ip->ttl       = 64;
    ip->protocol  = IP_PROTO_UDP;
    ip->src_ip    = 0;
    ip->dst_ip    = htonl(0xFFFFFFFF);
    udp_hdr_t *udp = (udp_hdr_t *)(ip + 1);
    udp->src_port = htons(DHCP_CLIENT_PORT);
    udp->dst_port = htons(DHCP_SERVER_PORT);
    udp->checksum = 0;
    dhcp_msg_t *dhcp = (dhcp_msg_t *)(udp + 1);
    memset(dhcp, 0, sizeof(dhcp_msg_t));
    dhcp->op      = 1;
    dhcp->htype   = 1;
    dhcp->hlen    = 6;
    dhcp->xid     = xid;
    dhcp->flags   = htons(0x8000);
    memcpy(dhcp->chaddr, g_our_mac, 6);
    dhcp->magic   = htonl(DHCP_MAGIC);
    uint8_t *opt = dhcp->options;
    opt = dhcp_opt_byte(opt, DHCP_OPT_MSG_TYPE, DHCP_REQUEST);
    uint32_t nip = htonl(offered_ip);
    opt = dhcp_opt_data(opt, DHCP_OPT_REQ_IP, &nip, 4);
    uint32_t nsip = htonl(server_ip);
    opt = dhcp_opt_data(opt, DHCP_OPT_SRV_ID, &nsip, 4);
    opt = dhcp_opt_end(opt);
    uint16_t dhcp_len = (uint16_t)(uintptr_t)(opt - (uint8_t *)dhcp);
    uint16_t udp_len  = sizeof(udp_hdr_t) + dhcp_len;
    uint16_t ip_total = sizeof(ip_hdr_t) + udp_len;
    udp->len = htons(udp_len);
    ip->total_len = htons(ip_total);
    ip->checksum = ip_checksum(ip, sizeof(ip_hdr_t));
    return sizeof(eth_hdr_t) + ip_total;
}

static bool parse_dhcp_reply(const uint8_t *frame, uint16_t len,
                              uint32_t *yiaddr, uint32_t *router, uint32_t *dns)
{
    if (len < sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + 28)
        return false;
    const eth_hdr_t *eth = (const eth_hdr_t *)frame;
    if (eth->type != ETH_TYPE_IP) return false;
    const ip_hdr_t *ip = (const ip_hdr_t *)(eth + 1);
    if (ip->protocol != IP_PROTO_UDP) return false;
    const udp_hdr_t *udp = (const udp_hdr_t *)(ip + 1);
    if (htons(udp->src_port) != DHCP_SERVER_PORT) return false;
    if (htons(udp->dst_port) != DHCP_CLIENT_PORT) return false;
    const dhcp_msg_t *dhcp = (const dhcp_msg_t *)(udp + 1);
    uint16_t dhcp_len = htons(udp->len) - sizeof(udp_hdr_t);
    if (dhcp_len < 28) return false;
    if (dhcp->op != 2) return false;
    if (htonl(dhcp->magic) != DHCP_MAGIC) return false;
    *yiaddr = htonl(dhcp->yiaddr);
    if (*yiaddr == 0) return false;
    *router = 0;
    *dns = 0;
    const uint8_t *opt = dhcp->options;
    uint16_t opt_len = dhcp_len > (uint16_t)(sizeof(dhcp_msg_t) - sizeof(dhcp->options))
        ? dhcp_len - (uint16_t)(sizeof(dhcp_msg_t) - sizeof(dhcp->options)) : 0;
    while (opt_len > 0) {
        uint8_t tag = *opt++;
        opt_len--;
        if (tag == DHCP_OPT_END) break;
        if (tag == DHCP_OPT_PAD) continue;
        if (opt_len < 1) break;
        uint8_t olen = *opt++;
        opt_len--;
        if (opt_len < olen) break;
        if (tag == DHCP_OPT_ROUTER && olen >= 4) {
            *router = ((uint32_t)opt[0] << 24) | ((uint32_t)opt[1] << 16) |
                      ((uint32_t)opt[2] << 8) | (uint32_t)opt[3];
        }
        if (tag == DHCP_OPT_DNS && olen >= 4) {
            *dns = ((uint32_t)opt[0] << 24) | ((uint32_t)opt[1] << 16) |
                   ((uint32_t)opt[2] << 8) | (uint32_t)opt[3];
        }
        opt += olen;
        opt_len -= olen;
    }
    return true;
}

bool net_stack_dhcp_discover(void)
{
    if (!g_send_fn || !g_poll_fn) return false;

    uint32_t xid = (uint32_t)(uintptr_t)g_our_mac;
    xid ^= (uint32_t)time_uptime_seconds();

    /* ── Send DHCPDISCOVER ── */
    uint8_t frame[2048];
    uint16_t flen = build_dhcp_discover(frame, xid);
    g_send_fn(frame, flen);

    /* ── Poll for DHCPOFFER (up to 3 seconds) ── */
    uint32_t offered_ip = 0, server_ip = 0, router_ip = 0, dns_ip = 0;
    uint64_t deadline = time_uptime_seconds() + 3;
    bool got_offer = false;

    while (time_uptime_seconds() < deadline) {
        uint8_t rbuf[2048];
        int n = g_poll_fn(rbuf, sizeof(rbuf));
        if (n <= 0) { sched_yield(); continue; }
        if (net_stack_rx(rbuf, (uint16_t)n)) continue;

        uint32_t yi = 0, ro = 0, dn = 0;
        if (parse_dhcp_reply(rbuf, (uint16_t)n, &yi, &ro, &dn)) {
            offered_ip = yi;
            router_ip  = ro;
            dns_ip     = dn;
            /* Extract server IP from DHCP message siaddr */
            const eth_hdr_t *re = (const eth_hdr_t *)rbuf;
            if (n >= (int)(sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + 28)) {
                const dhcp_msg_t *rdhcp = (const dhcp_msg_t *)
                    ((const udp_hdr_t *)((const ip_hdr_t *)(re + 1) + 1) + 1);
                server_ip = htonl(rdhcp->siaddr);
            }
            got_offer = true;
            break;
        }
    }

    if (!got_offer) {
        return false;
    }

    /* ── Send DHCPREQUEST ── */
    flen = build_dhcp_request(frame, xid, offered_ip, server_ip);
    g_send_fn(frame, flen);

    /* ── Poll for DHCPACK (up to 2 seconds) ── */
    deadline = time_uptime_seconds() + 2;
    bool got_ack = false;
    uint32_t ack_router = 0, ack_dns = 0;

    while (time_uptime_seconds() < deadline) {
        uint8_t rbuf[2048];
        int n = g_poll_fn(rbuf, sizeof(rbuf));
        if (n <= 0) { sched_yield(); continue; }
        if (net_stack_rx(rbuf, (uint16_t)n)) continue;

        uint32_t yi = 0, ro = 0, dn = 0;
        if (parse_dhcp_reply(rbuf, (uint16_t)n, &yi, &ro, &dn)) {
            if (yi == offered_ip) {
                ack_router = ro;
                ack_dns    = dn;
                got_ack = true;
                break;
            }
        }
    }

    if (got_ack) {
        g_net_if_ip      = offered_ip;
        if (ack_router)   g_net_gateway    = ack_router;
        if (ack_dns)      g_net_dns_server = ack_dns;
    } else {
        g_net_if_ip      = offered_ip;
        if (router_ip)    g_net_gateway    = router_ip;
        if (dns_ip)       g_net_dns_server = dns_ip;
    }

    return true;
}


