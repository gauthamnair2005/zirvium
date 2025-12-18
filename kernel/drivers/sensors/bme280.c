/*
 * Bosch BME280 Environmental Sensor Driver
 * Zirvium OS - Temperature, Humidity, Pressure sensor
 */

#include <kernel/types.h>
#include <kernel/kernel.h>

#define BME280_I2C_ADDR 0x76

/* BME280 Registers */
#define BME280_REG_ID        0xD0
#define BME280_REG_RESET     0xE0
#define BME280_REG_CTRL_HUM  0xF2
#define BME280_REG_STATUS    0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG    0xF5
#define BME280_REG_PRESS_MSB 0xF7
#define BME280_REG_TEMP_MSB  0xFA
#define BME280_REG_HUM_MSB   0xFD

typedef struct {
    uint8_t i2c_addr;
    
    /* Calibration data */
    uint16_t dig_T1;
    int16_t dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t dig_H1, dig_H3;
    int16_t dig_H2, dig_H4, dig_H5;
    int8_t dig_H6;
    
    /* Current readings */
    int32_t temperature;  /* Celsius * 100 */
    uint32_t pressure;    /* Pa */
    uint32_t humidity;    /* % * 1024 */
} bme280_sensor_t;

static bme280_sensor_t bme280;

int bme280_init(void) {
    kprintf("BME280: Bosch Environmental Sensor\n");
    kprintf("BME280: Temperature + Humidity + Pressure\n");
    
    bme280.i2c_addr = BME280_I2C_ADDR;
    
    /* TODO: I2C communication to read chip ID */
    
    kprintf("BME280: Initialized on I2C 0x%x\n", bme280.i2c_addr);
    
    return 0;
}

int bme280_read_temperature(int32_t *temp) {
    /* TODO: Read and compensate temperature */
    *temp = bme280.temperature;
    return 0;
}

int bme280_read_pressure(uint32_t *press) {
    *press = bme280.pressure;
    return 0;
}

int bme280_read_humidity(uint32_t *hum) {
    *hum = bme280.humidity;
    return 0;
}

void bme280_cleanup(void) {
    kprintf("BME280: Driver unloaded\n");
}
