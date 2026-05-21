/* Zirvium VirtIO Block Driver
 * Ported from Linux drivers/block/virtio_blk.c
 * Legacy PCI mode, polling, single queue */
#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/pci_compat.h"
#include "drivers/zirv/device.h"
#include "virtio_ids.h"
#include "virtio_pci.h"
#include "virtio_ring.h"
#include "virtio_blk.h"
#include "kernel/console.h"
#include <stdint.h>
#include <string.h>

#define VIRTIO_BLK_QUEUE_SIZE 64

struct virtio_blk_dev {
    pci_dev_t        *pdev;
    void             *mmio;
    struct virtqueue *vq;
    void             *ring_mem;
    uint64_t          ring_phys;
    uint64_t          capacity;
    uint32_t          blk_size;
    uint16_t          num_queues;
    int               ro;
};

static void virtio_blk_set_status(struct virtio_blk_dev *dev, uint8_t status)
{
    iowrite8(status, dev->mmio + VIRTIO_PCI_DEVICE_STATUS);
}

static uint32_t virtio_blk_get_features(struct virtio_blk_dev *dev)
{
    return ioread32(dev->mmio + VIRTIO_PCI_HOST_FEATURES);
}

static void virtio_blk_set_features(struct virtio_blk_dev *dev, uint32_t features)
{
    iowrite32(features, dev->mmio + VIRTIO_PCI_GUEST_FEATURES);
}

static void virtio_blk_select_queue(struct virtio_blk_dev *dev, uint16_t q)
{
    iowrite16(q, dev->mmio + VIRTIO_PCI_QUEUE_SEL);
}

static uint16_t virtio_blk_get_queue_size(struct virtio_blk_dev *dev)
{
    return ioread16(dev->mmio + VIRTIO_PCI_QUEUE_NUM);
}

static void virtio_blk_set_queue_pfn(struct virtio_blk_dev *dev, uint32_t pfn)
{
    iowrite32(pfn, dev->mmio + VIRTIO_PCI_QUEUE_PFN);
}

static int virtio_blk_init_vq(struct virtio_blk_dev *dev)
{
    virtio_blk_select_queue(dev, 0);
    uint16_t qsz = virtio_blk_get_queue_size(dev);
    if (qsz == 0 || qsz > 256) return -1;
    klog(LOG_INFO, "VIRT", "virtio-blk: queue size %u", qsz);

    unsigned rsz = vring_size(qsz);
    dev->ring_mem = kzalloc(rsz, GFP_KERNEL);
    if (!dev->ring_mem) return -1;
    dev->ring_phys = (uint64_t)(uintptr_t)dev->ring_mem;
    dev->vq = vq_new(qsz, dev->ring_mem);
    if (!dev->vq) { kfree(dev->ring_mem); dev->ring_mem = NULL; return -1; }
    virtio_blk_set_queue_pfn(dev, (uint32_t)(dev->ring_phys >> 12));
    return 0;
}

static int virtio_blk_submit_rw(struct virtio_blk_dev *dev,
                                 uint32_t type, uint64_t sector,
                                 void *buf, unsigned int count)
{
    struct virtio_blk_req *req = kzalloc(sizeof(*req), GFP_KERNEL);
    uint8_t *status = kzalloc(1, GFP_KERNEL);
    if (!req || !status) { kfree(req); kfree(status); return -1; }

    req->type = type;
    req->sector = sector;

    uint64_t req_phys = (uint64_t)(uintptr_t)req;
    uint64_t buf_phys = (uint64_t)(uintptr_t)buf;
    uint64_t st_phys  = (uint64_t)(uintptr_t)status;

    vq_add(dev->vq, req_phys, sizeof(*req), 0);
    vq_add(dev->vq, buf_phys, count * dev->blk_size, type == VIRTIO_BLK_T_IN);
    vq_add(dev->vq, st_phys, 1, 1);

    vq_kick(dev->vq, dev->mmio);

    for (int i = 0; i < 100000; i++) {
        if (vq_get_buf(dev->vq, NULL) >= 0) break;
        __asm__("pause");
    }

    int ret = (*status == VIRTIO_BLK_S_OK) ? 0 : -1;
    kfree(req);
    kfree(status);
    return ret;
}

static int virtio_blk_read_sectors(device_desc_t *desc,
                                    uint64_t sector, uint32_t count, void *buf)
{
    struct virtio_blk_dev *dev = (struct virtio_blk_dev *)desc->driver_data;
    if (!dev) return -1;
    return virtio_blk_submit_rw(dev, VIRTIO_BLK_T_IN, sector, buf, count);
}

static int virtio_blk_write_sectors(device_desc_t *desc,
                                     uint64_t sector, uint32_t count,
                                     const void *buf)
{
    struct virtio_blk_dev *dev = (struct virtio_blk_dev *)desc->driver_data;
    if (!dev || dev->ro) return -1;
    return virtio_blk_submit_rw(dev, VIRTIO_BLK_T_OUT, sector, (void *)buf, count);
}

static dev_ops_t virtio_blk_ops = {
    .read_sectors  = virtio_blk_read_sectors,
    .write_sectors = virtio_blk_write_sectors,
};

static int virtio_blk_probe(pci_dev_t *pdev, const pci_device_id_t *id)
{
    (void)id;
    klog(LOG_INFO, "VIRT", "virtio-blk: probing %04x:%04x",
         pdev->vendor_id, pdev->device_id);

    struct virtio_blk_dev *dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) return -1;
    dev->pdev = pdev;
    pci_enable_device(pdev);
    dev->mmio = pci_map_bar(pdev, VIRTIO_PCI_LEGACY_BAR);
    if (!dev->mmio) { kfree(dev); return -1; }

    klog(LOG_INFO, "VIRT", "virtio-blk: MMIO at %p", dev->mmio);

    virtio_blk_set_status(dev, 0);
    virtio_blk_set_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE);
    virtio_blk_set_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

    uint32_t features = virtio_blk_get_features(dev);
    features &= (1 << VIRTIO_BLK_F_SIZE_MAX) | (1 << VIRTIO_BLK_F_SEG_MAX)
               | (1 << VIRTIO_BLK_F_GEOMETRY) | (1 << VIRTIO_BLK_F_BLK_SIZE)
               | (1 << VIRTIO_BLK_F_FLUSH) | (1 << VIRTIO_BLK_F_TOPOLOGY)
               | (1 << VIRTIO_BLK_F_RO);
    virtio_blk_set_features(dev, features);

    virtio_blk_set_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER
                               | VIRTIO_CONFIG_S_FEATURES_OK);

    if (virtio_blk_init_vq(dev) < 0) { kfree(dev); return -1; }

    virtio_blk_set_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER
                               | VIRTIO_CONFIG_S_FEATURES_OK | VIRTIO_CONFIG_S_DRIVER_OK);

    struct virtio_blk_config cfg;
    uint8_t *cfg_ptr = (uint8_t *)(dev->mmio + 20);
    memcpy(&cfg, cfg_ptr, sizeof(cfg));

    dev->capacity = cfg.capacity;
    dev->blk_size = cfg.blk_size ? cfg.blk_size : 512;
    dev->ro = !!(features & (1 << VIRTIO_BLK_F_RO));

    klog(LOG_OK, "VIRT", "virtio-blk: capacity=%llu sectors, blk_size=%u%s",
         (unsigned long long)dev->capacity, dev->blk_size,
         dev->ro ? " (RO)" : "");

    registered_device_t *reg = zirv_register_device(
        DEV_CLASS_NVME, DEV_CLASS_SSD, "VirtIO Block", &virtio_blk_ops);
    if (!reg || !reg->vnode) { kfree(dev); return -1; }

    reg->desc.driver_data = dev;
    dev->blk_size = 512;
    return 0;
}

static struct pci_device_id virtio_blk_ids[] = {
    { PCI_DEVICE(0x1AF4, 0x1001) },
    { PCI_DEVICE(0x1AF4, 0x1042) },
    { 0, }
};

struct pci_driver_linux virtio_blk_driver = {
    .name      = "virtio-blk",
    .id_table  = virtio_blk_ids,
    .probe     = virtio_blk_probe,
};
