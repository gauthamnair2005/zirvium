// RTC CMOS Driver for Zirvium
#include <kernel/driver.h>
#include <kernel/kernel.h>

#define RTC_PORT 0x70

static int rtc_cmos_probe(struct device *dev __attribute__((unused))) {
    kprintf("rtc: CMOS RTC initialized\n");
    return DRIVER_OK;
}

static int rtc_cmos_remove(struct device *dev __attribute__((unused))) {
    return DRIVER_OK;
}

static struct driver rtc_cmos_driver = {
    .name = "rtc_cmos",
    .probe = rtc_cmos_probe,
    .remove = rtc_cmos_remove,
};

void __init rtc_cmos_init(void) {
    driver_register(&rtc_cmos_driver);
}
