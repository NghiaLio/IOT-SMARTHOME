#include "sensors.h"
#include "config.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "driver/gpio.h"

static const char *TAG = "SENSORS";

float temperature = 0.0;
float humidity = 0.0;
float gas_level = 0.0;
int flame_detected = 0;
int rain_detected = 0;

adc_oneshot_unit_handle_t adc1_handle;

void init_adc(void) {
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config1, &adc1_handle);
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(adc1_handle, GAS_ADC_CHANNEL, &config);
}

esp_err_t dht_read_data(gpio_num_t pin, float *humidity, float *temperature, int timeout_ms) {
    uint8_t data[5] = {0,0,0,0,0};
    
    // 1. Gửi tín hiệu Start
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    ets_delay_us(20000); // Kéo thấp 20ms
    gpio_set_level(pin, 1);
    ets_delay_us(30);
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    // 2. Chờ phản hồi (Có Timeout để tránh treo)
    int timeout = 1000;
    while(gpio_get_level(pin) == 1) {
        if(timeout-- <= 0) return ESP_ERR_TIMEOUT;
        ets_delay_us(1);
    }
    
    timeout = 1000;
    while(gpio_get_level(pin) == 0) {
        if(timeout-- <= 0) return ESP_ERR_TIMEOUT;
        ets_delay_us(1);
    }
    
    timeout = 1000;
    while(gpio_get_level(pin) == 1) {
        if(timeout-- <= 0) return ESP_ERR_TIMEOUT;
        ets_delay_us(1);
    }

    // 3. Đọc 40 bit dữ liệu
    for (int i = 0; i < 40; i++) {
        timeout = 1000; 
        while(gpio_get_level(pin) == 0) {
            if(timeout-- <= 0) return ESP_ERR_TIMEOUT;
            ets_delay_us(1);
        }
        
        ets_delay_us(30); 
        if (gpio_get_level(pin)) {
            data[i/8] |= (1 << (7 - (i%8)));
        }
        
        timeout = 1000;
        while(gpio_get_level(pin) == 1) {
            if(timeout-- <= 0) return ESP_ERR_TIMEOUT;
            ets_delay_us(1);
        }
    }

    // 4. Checksum
    if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        *humidity = data[0];
        *temperature = data[2];
        return ESP_OK;
    } else {
        return ESP_ERR_INVALID_CRC;
    }
}
