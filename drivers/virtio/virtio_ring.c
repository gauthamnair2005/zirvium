/* Zirvium VirtIO ring implementation
 * Ported from Linux kernel drivers/virtio/virtio_ring.c
 * Simplified for single-queue, legacy mode, polling operation */
#include "drivers/compat/linux_compat.h"
#include "virtio_ring.h"
#include "virtio_pci.h"
#include <stdint.h>
#include <string.h>

struct virtqueue {
    struct vring vring;
    unsigned int num_free;
    unsigned int free_head;
    uint16_t free_desc[256];
    uint16_t avail_idx_shadow;
    void *priv;
};

struct virtqueue *vq_new(unsigned int num, void *ring_mem)
{
    if (num > 256) num = 256;
    struct virtqueue *vq = kzalloc(sizeof(*vq), GFP_KERNEL);
    if (!vq) return NULL;
    memset(vq, 0, sizeof(*vq));
    vring_init(&vq->vring, num, ring_mem);
    vq->num_free = num;
    vq->free_head = 0;
    vq->avail_idx_shadow = 0;
    for (unsigned int i = 0; i < num; i++) {
        vq->free_desc[i] = i;
        vq->vring.desc[i].next = (i + 1 < num) ? i + 1 : 0;
        vq->vring.desc[i].flags = VRING_DESC_F_NEXT;
    }
    if (num > 0)
        vq->vring.desc[num - 1].flags = 0;
    return vq;
}

static unsigned vq_alloc_desc(struct virtqueue *vq)
{
    if (vq->num_free == 0) return (unsigned)-1;
    unsigned id = vq->free_head;
    vq->free_head = vq->vring.desc[id].next;
    vq->num_free--;
    return id;
}

int vq_add(struct virtqueue *vq, uint64_t phys_addr, uint32_t len, int write)
{
    unsigned id = vq_alloc_desc(vq);
    if (id == (unsigned)-1) return -1;
    vq->vring.desc[id].addr = phys_addr;
    vq->vring.desc[id].len = len;
    vq->vring.desc[id].flags = write ? VRING_DESC_F_WRITE : 0;
    vq->vring.desc[id].next = 0;
    vq->vring.avail->ring[vq->vring.avail->idx % vq->vring.num] = id;
    vq->vring.avail->idx++;
    vq->avail_idx_shadow = vq->vring.avail->idx;
    return 0;
}

void vq_kick(struct virtqueue *vq, void *mmio)
{
    wmb();
    iowrite16(vq->vring.avail->idx - 1, mmio + VIRTIO_PCI_QUEUE_NOTIFY);
}

int vq_get_buf(struct virtqueue *vq, uint32_t *len)
{
    u16 last_used = vq->vring.used->idx;
    if (vq->avail_idx_shadow == last_used)
        return -1;
    uint16_t id = vq->vring.used->ring[0].id;
    if (len)
        *len = vq->vring.used->ring[0].len;
    vq->vring.desc[id].addr = 0;
    vq->vring.desc[id].len = 0;
    vq->vring.desc[id].flags = VRING_DESC_F_NEXT;
    vq->vring.desc[id].next = vq->free_head;
    vq->free_head = id;
    vq->num_free++;
    vq->avail_idx_shadow = last_used;
    vq->vring.used->idx++;
    return (int)id;
}

void vq_free(struct virtqueue *vq)
{
    kfree(vq);
}
