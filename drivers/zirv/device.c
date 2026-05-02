/* drivers/zirv/device.c
 * Zirvium Kernel — /zirv device registry implementation
 */
#include "device.h"
#include "../../fs/mosix.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* ── Static slab for device entries ──────────────────────────────────────── */
#define MAX_DEVICES  256
static registered_device_t device_pool[MAX_DEVICES];
static uint32_t device_count_total = 0;

/* Per-(bus,media) instance counters */
#define COUNTER_BUCKETS  (DEV_CLASS_MAX * DEV_CLASS_MAX)
static uint32_t instance_counter[COUNTER_BUCKETS];

static uint32_t counter_key(dev_class_t bus, dev_class_t media)
{
    return (uint32_t)bus * DEV_CLASS_MAX + (uint32_t)media;
}

/* ── Public implementation ──────────────────────────────────────────────── */

void zirv_dev_init(void)
{
    memset(device_pool,      0, sizeof(device_pool));
    memset(instance_counter, 0, sizeof(instance_counter));
    device_count_total = 0;
}

registered_device_t *zirv_register_device(dev_class_t bus, dev_class_t media,
                                           const char *model,
                                           const dev_ops_t *ops)
{
    if (device_count_total >= MAX_DEVICES) return NULL;

    registered_device_t *entry = &device_pool[device_count_total++];
    entry->ops = ops;

    /* Fill in descriptor */
    device_desc_t *desc  = &entry->desc;
    desc->bus_class      = bus;
    desc->media_class    = media;
    desc->present        = true;
    desc->driver_data    = NULL;

    if (model) {
        size_t mlen = strlen(model);
        if (mlen >= sizeof(desc->model)) mlen = sizeof(desc->model) - 1;
        memcpy(desc->model, model, mlen);
        desc->model[mlen] = '\0';
    }

    /* Assign major/minor numbers */
    desc->major = (uint32_t)bus;
    desc->minor = instance_counter[counter_key(bus, media)];

    /* Register in the VFS /zirv tree */
    entry->vnode = vfs_register_device(bus, media, desc->minor, desc);

    /* Increment instance counter */
    instance_counter[counter_key(bus, media)]++;

    /* Run driver init if provided */
    if (ops && ops->init)
        ops->init(desc);

    /* Link into singly-linked list (self-links at tail via device_pool) */
    if (device_count_total >= 2) {
        device_pool[device_count_total - 2].next = entry;
    }
    entry->next = NULL;

    return entry;
}

registered_device_t *zirv_find_device(dev_class_t bus, dev_class_t media,
                                       uint32_t index)
{
    uint32_t found = 0;
    for (uint32_t i = 0; i < device_count_total; i++) {
        registered_device_t *d = &device_pool[i];
        if (d->desc.bus_class == bus && d->desc.media_class == media) {
            if (found == index) return d;
            found++;
        }
    }
    return NULL;
}

uint32_t zirv_device_count(dev_class_t bus, dev_class_t media)
{
    return instance_counter[counter_key(bus, media)];
}

void zirv_foreach_device(bool (*cb)(registered_device_t *dev, void *ctx),
                          void *ctx)
{
    for (uint32_t i = 0; i < device_count_total; i++) {
        if (!cb(&device_pool[i], ctx)) return;
    }
}
