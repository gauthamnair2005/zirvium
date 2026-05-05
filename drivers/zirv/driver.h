#ifndef ZIRVIUM_DRIVERS_ZIRV_DRIVER_H
#define ZIRVIUM_DRIVERS_ZIRV_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    DRIVER_TYPE_PCI,
    DRIVER_TYPE_PLATFORM
} driver_type_t;

typedef struct zirv_driver {
    const char *name;
    const char *component_tag;
    driver_type_t type;
    
    union {
        struct {
            uint16_t vendor_id;
            uint16_t device_id;
        } pci;
        struct {
            const char *acpi_hid;
        } platform;
    } match;

    /**
     * probe - attempt to initialize the driver for the given hardware.
     * @hw_desc: pointer to pci_dev_t or other hardware descriptor.
     * Returns 0 on success, negative on failure.
     */
    int (*probe)(void *hw_desc);
} zirv_driver_t;

/**
 * driver_register - register a new driver with the kernel.
 */
void driver_register(const zirv_driver_t *drv);

/**
 * driver_probe_all - scan all buses and attempt to load drivers for 
 * detected hardware.
 */
void driver_probe_all(void);

#endif /* ZIRVIUM_DRIVERS_ZIRV_DRIVER_H */
