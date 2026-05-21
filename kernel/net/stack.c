#include "stack.h"
#include <stdint.h>
#include <string.h>

/* ── Packet header structures ────────────────────────────────────────────── */

typedef struct eth_hdr {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t type;
} __attribute__((packed)) eth_hdr_t;

#define ETH_TYPE_ARP  0x0608  /* 0x0806 in network byte order (little-endian) */
#define ETH_TYPE_IP   0x0008  /* 0x0800 */

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
#define ARP_PTYPE_IP   0x0008  /* 0x0800 */
#define ARP_OP_REQUEST 0x0100  /* 1 in network byte order */
#define ARP_OP_REPLY   0x0200  /* 2 */

/* ── Static state ────────────────────────────────────────────────────────── */

static uint8_t     g_our_mac[NET_MAC_ALEN];
static net_send_fn g_send_fn = NULL;

/* ── Public API ──────────────────────────────────────────────────────────── */

void net_stack_init(const uint8_t mac[NET_MAC_ALEN])
{
    memcpy(g_our_mac, mac, NET_MAC_ALEN);
}

void net_stack_set_send(net_send_fn fn)
{
    g_send_fn = fn;
}

/* ── ARP packet handler ──────────────────────────────────────────────────── */

static int handle_arp(const eth_hdr_t *eth, uint16_t len)
{
    if (len < sizeof(eth_hdr_t) + sizeof(arp_pkt_t)) return 0;
    const arp_pkt_t *arp = (const arp_pkt_t *)(eth + 1);

    if (arp->hw_type != ARP_HTYPE_ETH)        return 0;
    if (arp->proto_type != ARP_PTYPE_IP)       return 0;
    if (arp->hw_len != NET_MAC_ALEN)           return 0;
    if (arp->proto_len != NET_IP_ALEN)         return 0;

    /* Check if the request is for us (target IP matches our assigned IP) */
    /* For now: respond to any ARP request for 10.0.2.15 */
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
        /* IP packets pass through to user-space */
        return 0;
    default:
        return 0;
    }
}
