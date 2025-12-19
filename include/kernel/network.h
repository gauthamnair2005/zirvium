#ifndef _KERNEL_NETWORK_H
#define _KERNEL_NETWORK_H

#include <stdint.h>

struct net_device {
    char name[16];
    uint8_t mac_addr[6];
    void *priv;
};

int register_netdev(struct net_device *dev);
void unregister_netdev(struct net_device *dev);

#endif
