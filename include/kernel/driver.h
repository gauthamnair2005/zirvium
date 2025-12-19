#ifndef _KERNEL_DRIVER_H
#define _KERNEL_DRIVER_H

#include <kernel/types.h>

/* Init/exit section attributes */
#define __init __attribute__((__section__(".init.text")))
#define __exit __attribute__((__section__(".exit.text")))

/* Forward declarations */
struct device;
struct driver;

/* Device structure */
struct device {
    char name[64];
    uint32_t type;
    void *driver_data;
    struct driver *driver;
};

/* Driver structure */
struct driver {
    const char *name;
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
};

/* Driver registration functions */
int driver_register(struct driver *drv);
void driver_unregister(struct driver *drv);

/* Device registration functions */
int device_register(struct device *dev);
void device_unregister(struct device *dev);

/* Driver subsystem functions */
void driver_subsystem_init(void);
int register_driver(const char *name);
void unregister_driver(const char *name);
int get_driver_count(void);
void list_drivers(void);

/* Driver error codes */
#define DRIVER_OK           0
#define DRIVER_ERROR       -1
#define DRIVER_NO_DEVICE   -2
#define DRIVER_NO_MEMORY   -3
#define DRIVER_BUSY        -4

#endif /* _KERNEL_DRIVER_H */
