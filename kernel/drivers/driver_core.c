/*
 * Zirvium Driver Core Framework
 * Provides infrastructure for device driver management
 */

#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/memory.h>

#define MAX_DRIVERS 512

typedef struct driver_entry {
    const char *name;
    const char *version;
    int (*init)(void);
    void (*exit)(void);
    int loaded;
    struct driver_entry *next;
} driver_entry_t;

static driver_entry_t *driver_list = NULL;
static int driver_count = 0;

void driver_subsystem_init(void) {
    driver_list = NULL;
    driver_count = 0;
    kprintf("Driver subsystem: Ready to load modules\n");
}

int register_driver(const char *name) {
    if (driver_count >= MAX_DRIVERS) {
        kprintf("Driver subsystem: Maximum drivers reached\n");
        return -1;
    }
    
    driver_entry_t *entry = (driver_entry_t*)kmalloc(sizeof(driver_entry_t));
    if (!entry) {
        return -1;
    }
    
    entry->name = name;
    entry->loaded = 1;
    entry->next = driver_list;
    driver_list = entry;
    driver_count++;
    
    return 0;
}

void unregister_driver(const char *name) {
    (void)name;
    if (driver_count > 0) {
        driver_count--;
    }
}

int get_driver_count(void) {
    return driver_count;
}

void list_drivers(void) {
    kprintf("Loaded drivers (%d):\n", driver_count);
    driver_entry_t *current = driver_list;
    int i = 1;
    while (current) {
        kprintf("  %d. %s\n", i++, current->name);
        current = current->next;
    }
}
