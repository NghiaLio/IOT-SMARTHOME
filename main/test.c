#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "SERVO_TEST";

// --- CẤU HÌNH ---
#define SERVO_PIN       14      // Chân dây cam nối vào GPIO 14
#define SERVO_FREQ      50      // Tần số bắt buộc cho Servo (50Hz)
#define DELAY_MS        2      // Tốc độ quay (30ms/độ -> Càng lớn càng chậm)

// Cấu hình Timer PWM (Độ phân giải 13 bit)
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_RES        LEDC_TIMER_13_BIT // Giá trị max là 8191

// Hàm đổi từ GÓC (0-180) sang DUTY CYCLE
uint32_t angle_to_duty(int angle) {
    // Xung chuẩn cho Servo SG90/MG996: 500us (0 độ) -> 2500us (180 độ)
    // Chu kỳ 50Hz = 20000us
    
    // 1. Tính độ rộng xung (us)
    uint32_t pulse_width = 500 + ((angle * (2500 - 500)) / 180);
    
    // 2. Đổi sang giá trị Duty (Thang đo 8191)
    // Duty = (pulse_width / 20000) * 8191
    uint32_t duty = (pulse_width * 8191) / 20000;
    
    return duty;
}

// Hàm điều khiển servo
void set_angle(int angle) {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, angle_to_duty(angle));
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void app_main(void) {
    // 1. Cấu hình Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_RES,
        .freq_hz          = SERVO_FREQ, 
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // 2. Cấu hình Channel (Gắn chân GPIO)
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    ESP_LOGI(TAG, "--- BAT DAU TEST SERVO ---");

    while (1) {
        // QUAY LÊN (0 -> 180)
        ESP_LOGI(TAG, "Dang quay len 180...");
        for (int i = 0; i <= 180; i++) {
            set_angle(i);
            vTaskDelay(pdMS_TO_TICKS(DELAY_MS)); // Chậm lại để quan sát
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Nghỉ 2 giây

        // QUAY VỀ (180 -> 0)
        ESP_LOGI(TAG, "Dang quay ve 0...");
        for (int i = 180; i >= 0; i--) {
            set_angle(i);
            vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // Nghỉ 2 giây
    }
}