/*
 * Zirvium Driver Core
 */
#include <kernel/types.h>
#include <kernel/driver.h>
#include <kernel/printk.h>

int driver_register(struct driver *drv) {
    if (!drv) return -1;
    printk(KERN_INFO "Driver registered: %s\n", drv->name);
    return 0;
}

void driver_unregister(struct driver *drv) {
    if (!drv) return;
    printk(KERN_INFO "Driver unregistered: %s\n", drv->name);
}
