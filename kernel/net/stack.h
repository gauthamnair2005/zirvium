#ifndef ZIRVIUM_NET_STACK_H
#define ZIRVIUM_NET_STACK_H

#include <stdint.h>

#define NET_IP_ALEN  4
#define NET_MAC_ALEN 6

/* Default QEMU SLiRP guest IP */
#define NET_GUEST_IP      ((10u << 24) | (0u << 16) | (2u << 8) | 15u)
#define NET_GATEWAY_IP    ((10u << 24) | (0u << 16) | (2u << 8) | 2u)

/* Callback: send a raw Ethernet frame */
typedef int (*net_send_fn)(const void *data, uint16_t len);

/* Initialise the network stack with our MAC address */
void net_stack_init(const uint8_t mac[NET_MAC_ALEN]);

/* Set the stack's transmit function (called by the driver) */
void net_stack_set_send(net_send_fn fn);

/* Process a received Ethernet frame (called by the driver's RX path).
 * Returns 1 if the frame was consumed (e.g. ARP reply), 0 if not. */
int net_stack_rx(const uint8_t *frame, uint16_t len);

#endif
