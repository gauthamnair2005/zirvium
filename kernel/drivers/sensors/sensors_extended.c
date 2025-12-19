#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int ambient_light_init(void) { kprintf("Sensor: Ambient light (APDS9960/TSL2561)\n"); return 0; }
int proximity_init(void) { kprintf("Sensor: Proximity sensor\n"); return 0; }
int hall_effect_init(void) { kprintf("Sensor: Hall effect (magnetic)\n"); return 0; }
int fingerprint_init(void) { kprintf("Sensor: Fingerprint scanner\n"); return 0; }
int heart_rate_init(void) { kprintf("Sensor: Heart rate monitor\n"); return 0; }
int step_counter_init(void) { kprintf("Sensor: Pedometer/step counter\n"); return 0; }
int uv_sensor_init(void) { kprintf("Sensor: UV light sensor\n"); return 0; }
