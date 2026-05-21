#ifndef _VIRTIO_RING_H
#define _VIRTIO_RING_H

#include <stdint.h>

#define VRING_DESC_F_NEXT      1
#define VRING_DESC_F_WRITE     2
#define VRING_DESC_F_INDIRECT  4

#define VRING_USED_F_NO_NOTIFY    1
#define VRING_AVAIL_F_NO_INTERRUPT 1

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

struct vring {
    unsigned int num;
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct vring_used *used;
};

static inline void vring_init(struct vring *vr, unsigned int num, void *p)
{
    vr->num = num;
    vr->desc = (struct vring_desc *)p;
    vr->avail = (struct vring_avail *)((char *)p + num * sizeof(struct vring_desc));
    vr->used = (struct vring_used *)(((unsigned long)&vr->avail->ring[num] + sizeof(uint16_t) + 3) & ~3UL);
}

static inline unsigned vring_size(unsigned int num)
{
    return ((sizeof(struct vring_desc) * num + sizeof(uint16_t) * (3 + num) + 3) & ~3)
           + sizeof(uint16_t) * 3 + sizeof(struct vring_used_elem) * num;
}

struct virtqueue;

struct virtqueue *vq_new(unsigned int num, void *ring_mem);
int vq_add(struct virtqueue *vq, uint64_t phys_addr, uint32_t len, int write);
void vq_kick(struct virtqueue *vq, void *mmio);
int vq_get_buf(struct virtqueue *vq, uint32_t *len);
void vq_free(struct virtqueue *vq);

#endif
