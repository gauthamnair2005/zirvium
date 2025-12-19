#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int rpi_gpio_init(void) { kprintf("RPi: GPIO controller\n"); return 0; }
int rpi_i2c_init(void) { kprintf("RPi: I2C bus\n"); return 0; }
int rpi_spi_init(void) { kprintf("RPi: SPI bus\n"); return 0; }
int rpi_uart_init(void) { kprintf("RPi: UART/serial\n"); return 0; }
int rpi_pwm_init(void) { kprintf("RPi: PWM controller\n"); return 0; }
int rpi_videocore_init(void) { kprintf("RPi: VideoCore IV/VI GPU\n"); return 0; }
int rpi_bcm2835_init(void) { kprintf("RPi: BCM2835/2836/2837/2711 SoC\n"); return 0; }
