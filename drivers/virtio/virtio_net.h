#ifndef _VIRTIO_NET_H
#define _VIRTIO_NET_H

#define VIRTIO_NET_F_CSUM           0
#define VIRTIO_NET_F_GUEST_CSUM     1
#define VIRTIO_NET_F_MTU            3
#define VIRTIO_NET_F_MAC            5
#define VIRTIO_NET_F_GUEST_TSO4     7
#define VIRTIO_NET_F_GUEST_TSO6     8
#define VIRTIO_NET_F_GUEST_ECN      9
#define VIRTIO_NET_F_GUEST_UFO     10
#define VIRTIO_NET_F_HOST_TSO4     11
#define VIRTIO_NET_F_HOST_TSO6     12
#define VIRTIO_NET_F_HOST_ECN      13
#define VIRTIO_NET_F_HOST_UFO      14
#define VIRTIO_NET_F_MRG_RXBUF     15
#define VIRTIO_NET_F_STATUS        16
#define VIRTIO_NET_F_CTRL_VQ       17

#define VIRTIO_NET_S_LINK_UP       1

#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1

struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed));

#define VIRTIO_NET_HDR_SIZE (sizeof(struct virtio_net_hdr))

struct virtio_net_config {
    uint8_t  mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
} __attribute__((packed));

#endif
