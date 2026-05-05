#include "driver.h"
#include "kernel/console.h"
#include "kernel/mm/vmm.h"
#include <stdint.h>

#define TPM_MMIO_BASE 0xFED40000ULL

static int tpm2_probe(void *hw_desc) {
    (void)hw_desc;

    /* Map TPM MMIO region */
    void *tpm_base = PHYS_TO_VIRT(TPM_MMIO_BASE);
    
    /* Basic check: read TPM_ACCESS register (offset 0) */
    uint8_t access = *(volatile uint8_t *)tpm_base;
    
    if (access == 0xFF) {
        return -1; /* No TPM found at this address */
    }

    klog(LOG_OK, "TPM ", "TPM 2.0 detected at %p (access: %02x)", TPM_MMIO_BASE, access);
    return 0;
}

const zirv_driver_t g_tpm2_driver = {
    .name = "tpm2",
    .component_tag = "TPM ",
    .type = DRIVER_TYPE_PLATFORM,
    .match.platform = { .acpi_hid = "MSFT0101" },
    .probe = tpm2_probe
};
