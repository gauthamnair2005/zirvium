// PN533 NFC Controller Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

static int nfc_pn533_probe(struct device *dev __attribute__((unused))) {
    kprintf("nfc: PN533 NFC controller initialized\n");
    return DRIVER_OK;
}

static int nfc_pn533_remove(struct device *dev __attribute__((unused))) {
    return DRIVER_OK;
}

static struct driver nfc_pn533_driver = {
    .name = "pn533",
    .probe = nfc_pn533_probe,
    .remove = nfc_pn533_remove,
};

void __init nfc_pn533_init(void) {
    driver_register(&nfc_pn533_driver);
}
