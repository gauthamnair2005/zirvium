#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int wacom_init(void) { kprintf("Wacom: Tablet and stylus support (Intuos/Cintiq/Bamboo)\n"); return 0; }
int wacom_usb_init(void) { kprintf("Wacom: USB tablets\n"); return 0; }
int wacom_bluetooth_init(void) { kprintf("Wacom: Bluetooth tablets\n"); return 0; }
