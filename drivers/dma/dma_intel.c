// Intel DMA Engine Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

static int dma_intel_probe(struct device *dev __attribute__((unused))) {
    kprintf("dma: Intel DMA engine initialized\n");
    return DRIVER_OK;
}

static int dma_intel_remove(struct device *dev __attribute__((unused))) {
    return DRIVER_OK;
}

static struct driver dma_intel_driver = {
    .name = "dma_intel",
    .probe = dma_intel_probe,
    .remove = dma_intel_remove,
};

void __init dma_intel_init(void) {
    driver_register(&dma_intel_driver);
}
