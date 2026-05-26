#ifndef ZIRVIUM_NET_STACK_H
#define ZIRVIUM_NET_STACK_H

#include <stdint.h>
#include <stdbool.h>

#define NET_IP_ALEN  4
#define NET_MAC_ALEN 6

/* Default QEMU SLiRP guest IP — used as fallback if DHCP fails */
#define NET_GUEST_IP_DEFAULT      ((10u << 24) | (0u << 16) | (2u << 8) | 15u)
#define NET_GATEWAY_IP_DEFAULT    ((10u << 24) | (0u << 16) | (2u << 8) | 2u)
#define NET_DNS_SERVER_IP_DEFAULT ((10u << 24) | (0u << 16) | (2u << 8) | 3u)

/* Mutable network configuration — updated by DHCP at boot */
extern uint32_t g_net_if_ip;
extern uint32_t g_net_gateway;
extern uint32_t g_net_dns_server;

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

/* Run a one-shot DHCP discover at boot to configure g_net_if_ip,
 * g_net_gateway, and g_net_dns_server.  Non-blocking after a brief wait.
 * Returns true if DHCP succeeded, false if defaults remain. */
bool net_stack_dhcp_discover(void);

/* Resolve a target IP to a MAC address, routing external IPs through
 * the default gateway.  Returns 0 on success with 6 bytes in out_mac,
 * or -1 on failure. */
int net_stack_route_resolve(uint32_t target_ip, uint8_t *out_mac);

#endif
