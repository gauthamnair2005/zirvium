/* drivers/zirv/nvme.h / nvme.c
 * Zirvium Kernel — NVMe PCIe SSD driver (minimal bring-up)
 *
 * Registers any NVMe namespaces it discovers as /zirv/nvme/ssd<n>.
 *
 * This is a polling-mode (no MSI-X) implementation suited for early boot.
 * A full interrupt-driven driver would replace this once the APIC is up.
 */
#ifndef ZIRVIUM_DRIVERS_ZIRV_NVME_H
#define ZIRVIUM_DRIVERS_ZIRV_NVME_H

/**
 * nvme_init - probe the system PCI bus for NVMe controllers and register
 *             each detected namespace with the /zirv device registry.
 */
void nvme_init(void);

#endif /* ZIRVIUM_DRIVERS_ZIRV_NVME_H */
