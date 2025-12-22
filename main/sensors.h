#ifndef SENSORS_H
#define SENSORS_H

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

// Sensor data
extern float temperature;
extern float humidity;
extern float gas_level;
extern int flame_detected;
extern int rain_detected;

// ADC handle
extern adc_oneshot_unit_handle_t adc1_handle;

// Initialize ADC for gas sensor
void init_adc(void);

// Read DHT11 sensor
esp_err_t dht_read_data(gpio_num_t pin, float *humidity, float *temperature, int timeout_ms);

#endif // SENSORS_H
