/* drivers/zirv/device.h
 * Zirvium Kernel — /zirv device registration & management
 *
 * Every physical device that the kernel discovers is registered through
 * this interface, which calls into the MOSIX VFS to create the correct
 * node under /zirv/<bus>/<type><index>.
 */
#ifndef ZIRVIUM_DRIVERS_ZIRV_DEVICE_H
#define ZIRVIUM_DRIVERS_ZIRV_DEVICE_H

#include "../../fs/mosix.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Per-device operations (driver vtable) ────────────────────────────────── */
typedef struct dev_ops {
    /**
     * init - initialise the hardware.
     * Returns 0 on success, negative errno on failure.
     */
    int  (*init)(device_desc_t *dev);

    /**
     * read_sectors - read @count LBA sectors from @lba into @buf.
     * @buf must be at least @count × 512 bytes.
     * Returns number of sectors actually read, or negative on error.
     */
    int  (*read_sectors)(device_desc_t *dev, uint64_t lba,
                         uint32_t count, void *buf);

    /**
     * write_sectors - write @count LBA sectors from @buf to @lba.
     * Returns number of sectors written, or negative on error.
     */
    int  (*write_sectors)(device_desc_t *dev, uint64_t lba,
                          uint32_t count, const void *buf);

    /**
     * ioctl - device-specific control operations.
     */
    int  (*ioctl)(device_desc_t *dev, uint32_t cmd, void *arg);

    /**
     * shutdown - quiesce the hardware before power-off / reboot.
     */
    void (*shutdown)(device_desc_t *dev);
} dev_ops_t;

/* ── Device registration entry ────────────────────────────────────────────── */
typedef struct registered_device {
    device_desc_t        desc;
    const dev_ops_t     *ops;
    vnode_t             *vnode;         /* back-pointer into /zirv tree */
    struct registered_device *next;
} registered_device_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * zirv_dev_init - set up the device registry (called once, before drivers).
 */
void zirv_dev_init(void);

/**
 * zirv_register_device - register a new physical device.
 *
 * @bus:    bus class (DEV_CLASS_SATA, DEV_CLASS_PATA, …)
 * @media:  media class (DEV_CLASS_HDD, DEV_CLASS_SSD, …)
 * @model:  human-readable model string
 * @ops:    pointer to the driver's operation vtable
 *
 * Returns a pointer to the registered_device_t on success, NULL on failure.
 * The returned descriptor is owned by the registry; callers must not free it.
 */
registered_device_t *zirv_register_device(dev_class_t bus, dev_class_t media,
                                           const char *model,
                                           const dev_ops_t *ops);

/**
 * zirv_find_device - look up a device by bus + media class and index.
 * Returns NULL if not found.
 */
registered_device_t *zirv_find_device(dev_class_t bus, dev_class_t media,
                                       uint32_t index);

/**
 * zirv_device_count - return number of registered devices of a given class.
 */
uint32_t zirv_device_count(dev_class_t bus, dev_class_t media);

/**
 * zirv_foreach_device - iterate over all registered devices.
 * @cb returns false to stop iteration early.
 */
void zirv_foreach_device(bool (*cb)(registered_device_t *dev, void *ctx),
                          void *ctx);

#endif /* ZIRVIUM_DRIVERS_ZIRV_DEVICE_H */
