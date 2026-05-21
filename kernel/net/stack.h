#ifndef ZIRVIUM_NET_STACK_H
#define ZIRVIUM_NET_STACK_H

#include <stdint.h>

#define NET_IP_ALEN  4
#define NET_MAC_ALEN 6

/* Default QEMU SLiRP guest IP */
#define NET_GUEST_IP      ((10u << 24) | (0u << 16) | (2u << 8) | 15u)
#define NET_GATEWAY_IP    ((10u << 24) | (0u << 16) | (2u << 8) | 2u)
#define NET_DNS_SERVER_IP ((10u << 24) | (0u << 16) | (2u << 8) | 3u)

/* Callback: send a raw Ethernet frame */
typedef int (*net_send_fn)(const void *data, uint16_t len);

/* Callback: poll for one raw Ethernet frame (non-blocking, returns 0 if none) */
typedef int (*net_poll_fn)(uint8_t *buf, uint16_t buflen);

/* Initialise the network stack with our MAC address */
void net_stack_init(const uint8_t mac[NET_MAC_ALEN]);

/* Set the stack's transmit function (called by the driver) */
void net_stack_set_send(net_send_fn fn);

/* Set the stack's poll function (called by the driver) */
void net_stack_set_poll(net_poll_fn fn);

/* Process a received Ethernet frame (called by the driver's RX path).
 * Returns 1 if the frame was consumed (e.g. ARP reply), 0 if not. */
int net_stack_rx(const uint8_t *frame, uint16_t len);

/* Resolve a domain name to an IPv4 address using DNS (blocking poll).
 * Returns the IP in host byte order on success, 0 on failure.
 * Uses the configured poll + send functions internally. */
uint32_t net_stack_dns_resolve(const char *domain);

#endif
