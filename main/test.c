#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BUZZER_GPIO 4 // Chọn chân GPIO kết nối Buzzer

static const char *TAG = "BUZZER_TEST";

void app_main(void) {
    ESP_LOGI(TAG, "Khoi tao Buzzer...");

    // 1. Reset chân (quan trọng với ESP32-S3)
    gpio_reset_pin(BUZZER_GPIO);
    
    // 2. Cấu hình chân làm Output
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        ESP_LOGI(TAG, "BEEP!");
        
        // Bật còi (Mức cao - 3.3V)
        gpio_set_level(BUZZER_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Kêu trong 500ms

        // Tắt còi (Mức thấp - 0V)
        gpio_set_level(BUZZER_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(1000)); // Nghỉ trong 500ms
    }
}