/* Zirvium VirtIO Net Driver
 * Ported from Linux drivers/net/virtio_net.c
 * Legacy PCI mode, polling, single queue */
#include "drivers/compat/linux_compat.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/pci_compat.h"
#include "drivers/zirv/device.h"
#include "virtio_ids.h"
#include "virtio_pci.h"
#include "virtio_ring.h"
#include "virtio_net.h"
#include "kernel/console.h"
#include <stdint.h>
#include <string.h>

#define VIRTIO_NET_QUEUE_SIZE 64
#define VIRTIO_NET_BUF_SIZE   2048

struct virtio_net_dev {
    pci_dev_t        *pdev;
    void             *mmio;
    struct virtqueue *tx_vq;
    struct virtqueue *rx_vq;
    void             *tx_ring_mem;
    void             *rx_ring_mem;
    uint64_t          tx_ring_phys;
    uint64_t          rx_ring_phys;
    uint8_t           mac[6];
    uint8_t           rx_buf[VIRTIO_NET_BUF_SIZE];
    uint8_t           tx_buf[VIRTIO_NET_BUF_SIZE];
    int               link_up;
    uint8_t           rx_hdr_buf[sizeof(struct virtio_net_hdr)];
};

static void virtio_net_set_status(struct virtio_net_dev *dev, uint8_t status)
{
    iowrite8(status, dev->mmio + VIRTIO_PCI_DEVICE_STATUS);
}

static uint32_t virtio_net_get_features(struct virtio_net_dev *dev)
{
    return ioread32(dev->mmio + VIRTIO_PCI_HOST_FEATURES);
}

static void virtio_net_set_features(struct virtio_net_dev *dev, uint32_t features)
{
    iowrite32(features, dev->mmio + VIRTIO_PCI_GUEST_FEATURES);
}

static void virtio_net_select_queue(struct virtio_net_dev *dev, uint16_t q)
{
    iowrite16(q, dev->mmio + VIRTIO_PCI_QUEUE_SEL);
}

static uint16_t virtio_net_get_queue_size(struct virtio_net_dev *dev)
{
    return ioread16(dev->mmio + VIRTIO_PCI_QUEUE_NUM);
}

static void virtio_net_set_queue_pfn(struct virtio_net_dev *dev, uint32_t pfn)
{
    iowrite32(pfn, dev->mmio + VIRTIO_PCI_QUEUE_PFN);
}

static int virtio_net_init_vqs(struct virtio_net_dev *dev)
{
    unsigned rsz;

    virtio_net_select_queue(dev, 0);
    uint16_t tx_qsz = virtio_net_get_queue_size(dev);
    if (tx_qsz == 0 || tx_qsz > 256) return -1;
    rsz = vring_size(tx_qsz);
    dev->tx_ring_mem = kzalloc(rsz, GFP_KERNEL);
    if (!dev->tx_ring_mem) return -1;
    dev->tx_ring_phys = (uint64_t)(uintptr_t)dev->tx_ring_mem;
    dev->tx_vq = vq_new(tx_qsz, dev->tx_ring_mem);
    if (!dev->tx_vq) { kfree(dev->tx_ring_mem); dev->tx_ring_mem = NULL; return -1; }
    virtio_net_set_queue_pfn(dev, (uint32_t)(dev->tx_ring_phys >> 12));

    virtio_net_select_queue(dev, 1);
    uint16_t rx_qsz = virtio_net_get_queue_size(dev);
    if (rx_qsz == 0 || rx_qsz > 256) return -1;
    rsz = vring_size(rx_qsz);
    dev->rx_ring_mem = kzalloc(rsz, GFP_KERNEL);
    if (!dev->rx_ring_mem) { kfree(dev->tx_ring_mem); return -1; }
    dev->rx_ring_phys = (uint64_t)(uintptr_t)dev->rx_ring_mem;
    dev->rx_vq = vq_new(rx_qsz, dev->rx_ring_mem);
    if (!dev->rx_vq) { kfree(dev->tx_ring_mem); kfree(dev->rx_ring_mem); return -1; }
    virtio_net_set_queue_pfn(dev, (uint32_t)(dev->rx_ring_phys >> 12));

    return 0;
}

static int virtio_net_send_frame(struct virtio_net_dev *dev,
                                  const void *data, uint16_t len)
{
    if (len + sizeof(struct virtio_net_hdr) > VIRTIO_NET_BUF_SIZE)
        return -1;

    struct virtio_net_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(dev->tx_buf, &hdr, sizeof(hdr));
    memcpy(dev->tx_buf + sizeof(hdr), data, len);

    uint64_t buf_phys = (uint64_t)(uintptr_t)dev->tx_buf;

    vq_add(dev->tx_vq, buf_phys, len + sizeof(hdr), 0);
    vq_kick(dev->tx_vq, dev->mmio);

    for (int i = 0; i < 100000; i++) {
        if (vq_get_buf(dev->tx_vq, NULL) >= 0) break;
        __asm__("pause");
    }
    return (int)len;
}

static int virtio_net_poll_frame(struct virtio_net_dev *dev,
                                  uint8_t *buf, uint16_t buflen)
{
    uint64_t buf_phys = (uint64_t)(uintptr_t)dev->rx_buf;
    uint64_t hdr_phys = (uint64_t)(uintptr_t)dev->rx_hdr_buf;

    vq_add(dev->rx_vq, hdr_phys, sizeof(struct virtio_net_hdr), 1);
    vq_add(dev->rx_vq, buf_phys, VIRTIO_NET_BUF_SIZE, 1);
    vq_kick(dev->rx_vq, dev->mmio);

    for (int i = 0; i < 100000; i++) {
        uint32_t len;
        int id = vq_get_buf(dev->rx_vq, &len);
        if (id >= 0) {
            if (len > sizeof(struct virtio_net_hdr)) {
                uint16_t pkt_len = len - sizeof(struct virtio_net_hdr);
                if (pkt_len > buflen) pkt_len = buflen;
                memcpy(buf, dev->rx_buf, pkt_len);
                return pkt_len;
            }
            break;
        }
        __asm__("pause");
    }
    return 0;
}

static int virtio_net_probe(pci_dev_t *pdev, const pci_device_id_t *id)
{
    (void)id;
    klog(LOG_INFO, "VIRT", "virtio-net: probing %04x:%04x",
         pdev->vendor_id, pdev->device_id);

    struct virtio_net_dev *dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) return -1;
    dev->pdev = pdev;
    pci_enable_device(pdev);
    dev->mmio = pci_map_bar(pdev, VIRTIO_PCI_LEGACY_BAR);
    if (!dev->mmio) { kfree(dev); return -1; }

    klog(LOG_INFO, "VIRT", "virtio-net: MMIO at %p", dev->mmio);

    virtio_net_set_status(dev, 0);
    virtio_net_set_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE);
    virtio_net_set_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

    uint32_t features = virtio_net_get_features(dev);
    features &= (1 << VIRTIO_NET_F_MAC) | (1 << VIRTIO_NET_F_STATUS)
              | (1 << VIRTIO_NET_F_CSUM);
    virtio_net_set_features(dev, features);

    virtio_net_set_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER
                               | VIRTIO_CONFIG_S_FEATURES_OK);

    if (virtio_net_init_vqs(dev) < 0) { kfree(dev); return -1; }

    virtio_net_set_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER
                               | VIRTIO_CONFIG_S_FEATURES_OK | VIRTIO_CONFIG_S_DRIVER_OK);

    struct virtio_net_config cfg;
    uint8_t *cfg_ptr = (uint8_t *)(dev->mmio + 20);
    memcpy(&cfg, cfg_ptr, sizeof(cfg));
    memcpy(dev->mac, cfg.mac, 6);

    dev->link_up = !!(cfg.status & VIRTIO_NET_S_LINK_UP);

    klog(LOG_OK, "VIRT", "virtio-net: MAC %02x:%02x:%02x:%02x:%02x:%02x%s",
         dev->mac[0], dev->mac[1], dev->mac[2],
         dev->mac[3], dev->mac[4], dev->mac[5],
         dev->link_up ? " (UP)" : " (DOWN)");

    registered_device_t *reg = zirv_register_device(
        DEV_CLASS_NET_ETH, DEV_CLASS_NET_ETH, "VirtIO Net", NULL);
    if (!reg || !reg->vnode) { kfree(dev); return -1; }
    reg->desc.driver_data = dev;

    return 0;
}

static struct pci_device_id virtio_net_ids[] = {
    { PCI_DEVICE(0x1AF4, 0x1000) },
    { PCI_DEVICE(0x1AF4, 0x1041) },
    { 0, }
};

struct pci_driver_linux virtio_net_driver = {
    .name      = "virtio-net",
    .id_table  = virtio_net_ids,
    .probe     = virtio_net_probe,
};
