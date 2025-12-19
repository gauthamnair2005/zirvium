#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int bt_headset_init(void) { kprintf("Bluetooth: Headset profile (HSP/HFP)\n"); return 0; }
int bt_a2dp_init(void) { kprintf("Bluetooth: A2DP audio streaming\n"); return 0; }
int bt_avrcp_init(void) { kprintf("Bluetooth: AVRCP media control\n"); return 0; }
