// Refactored Smart Home Control System
// Main orchestration file
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

// Include all module headers
#include "config.h"
#include "wifi_manager.h"
#include "firebase_handler.h"
#include "voice_handler.h"
#include "rfid_handler.h"
#include "sensors.h"
#include "actuators.h"
#include "i2s_mic.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

static const char *TAG = "SMART_HOME_MAIN";

// Biến trạng thái kết nối WiFi
volatile bool wifi_connected = false;

// Callback từ wifi_manager khi kết nối thành công
void wifi_manager_on_connected(void) {
    wifi_connected = true;
    ESP_LOGI(TAG, ">> WIFI CONNECTED! <<");
    blink_led();       // Đèn nháy
    buzzer_beep_short(); // Còi kêu
}

// ============================================================
// TASK CHÍNH
// ============================================================
void app_task(void *args) {
    ESP_LOGI(TAG, "Khoi dong he thong... (Cho 3s)");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "SAN SANG! Hay noi 'Bat den'...");

    // Buffer tạm để đọc mẫu kiểm tra độ ồn
    int16_t temp_buf[256];
    size_t r_bytes = 0;

    // Xả rác Mic lần đầu
    for(int i=0; i<20; i++) i2s_channel_read(rx_handle, temp_buf, sizeof(temp_buf), &r_bytes, 10);

    static int dht_read_counter = 0;
    static float prev_temperature = -1.0;
    static float prev_humidity = -1.0;
    static float prev_gas_level = -1.0;
    static int sensor_counter = 0;
    static int firebase_get_counter = 0;
    static int prev_flame_detected = -1;
    static int prev_rain_detected = -1;
    static int flame_debounce = 0;
    static int emergency = 0;
    static int rain_angle = 0;
    static int rain_debounce = 0;

    while (1) {
        // Đọc DHT11 mỗi 1 giây
        dht_read_counter++;
        if (dht_read_counter >= 50) {
            dht_read_counter = 0;
            esp_err_t result = dht_read_data(DHT_PIN, &humidity, &temperature, 1000);
            if (result == ESP_OK) {
                if (temperature != prev_temperature || humidity != prev_humidity) {
                    ESP_LOGI(TAG, "DHT11: Temp=%.1f C, Hum=%.1f %%", temperature, humidity);
                    prev_temperature = temperature;
                    prev_humidity = humidity;
                }
            } else {
                ESP_LOGE(TAG, "DHT11 read failed: %s", esp_err_to_name(result));
            }
        }
        
        // Đọc MQ2 Gas Sensor
        int gas_raw = 0;
        adc_oneshot_read(adc1_handle, GAS_ADC_CHANNEL, &gas_raw);
        gas_level = gas_raw;
        if (gas_level > 1500 && gas_level != prev_gas_level) {
            ESP_LOGW(TAG, "MQ2 Gas Level HIGH: %.0f", gas_level);
            prev_gas_level = gas_level;
            char json[64];
            sprintf(json, "{\"gasLevel\":%.0f}", gas_level);
            start_buzzer_alarm();
            if (!emergency) {
                emergency = 1;
                // Mở cửa khi phát hiện khí gas vượt ngưỡng
                set_servo_angle(90);
                door_angle = 90;
            }
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        } else if (gas_level <= 1500) {
            prev_gas_level = gas_level;
            int current_flame = !gpio_get_level(FLAME_PIN);
            if (!current_flame && emergency) {
                emergency = 0;
                // Đóng cửa khi hết gas và hết lửa
                set_servo_angle(0);
                door_angle = 0;
                stop_buzzer_alarm();
            }
        }
        
        // Đọc Flame Sensor
        int current_flame = !gpio_get_level(FLAME_PIN);
        if (current_flame == flame_detected) {
            flame_debounce++;
            if (flame_debounce >= 5) {
                if (flame_detected != prev_flame_detected) {
                    ESP_LOGW(TAG, "Flame Detected: %d", flame_detected);
                    prev_flame_detected = flame_detected;
                    if (flame_detected) {
                        start_buzzer_alarm();
                        if (!emergency) {
                            emergency = 1;
                            // Mở cửa khi phát hiện lửa
                            set_servo_angle(90);
                            door_angle = 90;
                        }
                    } else {
                        int gas_raw = 0;
                        adc_oneshot_read(adc1_handle, GAS_ADC_CHANNEL, &gas_raw);
                        if (gas_raw <= 1500 && emergency) {
                            emergency = 0;
                            // Đóng cửa khi hết lửa và hết gas
                            set_servo_angle(0);
                            door_angle = 0;
                            stop_buzzer_alarm();
                        }
                    }
                    char json[64];
                    sprintf(json, "{\"flameDetected\":%d}", flame_detected);
                    send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
                }
                flame_debounce = 0;
            }
        } else {
            flame_detected = current_flame;
            flame_debounce = 0;
        }
        
        // Đọc Rain Sensor
        int current_rain = !gpio_get_level(RAIN_PIN);
        if (current_rain == rain_detected) {
            rain_debounce++;
            if (rain_debounce >= 5) {
                if (rain_detected != prev_rain_detected) {
                    ESP_LOGW(TAG, "Rain Detected: %d", rain_detected);
                    prev_rain_detected = rain_detected;
                    // Control rain servo
                    if (rain_detected) {
                        set_rain_servo_angle(180);
                        rain_angle = 180;
                    } else {
                        set_rain_servo_angle(0);
                        rain_angle = 0;
                    }
                    char json[64];
                    sprintf(json, "{\"rainDetected\":%d,\"rainAngle\":%d}", rain_detected, rain_angle);
                    send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
                    // Gửi riêng rain_angle
                    char json_rain[32];
                    sprintf(json_rain, "{\"rainAngle\":%d}", rain_angle);
                    send_to_firebase(json_rain, HTTP_METHOD_PATCH, "data.json");
                }
                rain_debounce = 0;
            }
        } else {
            rain_detected = current_rain;
            rain_debounce = 0;
        }
        
        // Nhận dữ liệu từ Firebase mỗi 1 giây
        firebase_get_counter++;
        if (firebase_get_counter >= 50) {
            firebase_get_counter = 0;
            get_firebase_data();
        }
        
        // Gửi lên Firebase mỗi 10 giây
        sensor_counter++;
        if (sensor_counter >= 500) {
            sensor_counter = 0;
            char json[128];
            sprintf(json, "{\"temperature\":%.1f,\"humidity\":%.1f,\"gasLevel\":%.0f}", temperature, humidity, gas_level);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        }

        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

    // ============================================================
    // TASK XU LY GIONG NOI (tach rieng)
    // ============================================================
void voice_task(void *args) {
    int16_t temp_buf[256];
    size_t r_bytes = 0;

    while(1) {
        int vol = 0;
        i2s_channel_read(rx_handle, temp_buf, sizeof(temp_buf), &r_bytes, 100);

        for(int i = 0; i < 256; i++) {
            int abs_val = abs(temp_buf[i]);
            if(abs_val > vol) vol = abs_val;
        }

        if (vol > SOUND_THRESHOLD && wifi_connected) {
            ESP_LOGI(TAG, ">>> PHAT HIEN TIENG NOI (Vol: %d) <<<", vol);
            char *rec_buffer = (char *)calloc(1, RECORD_SIZE);
            if (rec_buffer == NULL) {
                ESP_LOGE(TAG, "Het RAM! Khong the ghi am.");
            } else {
                size_t bytes_recorded = 0;
                i2s_channel_read(rx_handle, rec_buffer, RECORD_SIZE, &bytes_recorded, portMAX_DELAY);
                send_to_wit_ai(rec_buffer, bytes_recorded);
                free(rec_buffer);
                ESP_LOGI(TAG, "Da giai phong RAM. Cho lenh moi...");
            }
            // Xả rác mic sau khi ghi âm
            for(int i=0; i<10; i++) i2s_channel_read(rx_handle, temp_buf, sizeof(temp_buf), &r_bytes, 10);
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ============================================================
// APP MAIN - Orchestrates all modules
// ============================================================
void app_main(void) {
    ESP_LOGI(TAG, "Initializing Smart Home System...");
    
    // Initialize NVS, netif và event loop cho WiFi Manager
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Initialize hardware modules
    init_rfid();
    init_led();
    init_servo();
    set_servo_angle(0);
    set_rain_servo_angle(0);
    init_adc();
    
    // Initialize WiFi với WiFi Manager (hỗ trợ cấu hình động)
    wifi_manager_start();
    
    // Chờ WiFi kết nối trước khi khởi tạo microphone
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "WiFi connected! Initializing microphone...");
    
    init_microphone();
    
    ESP_LOGI(TAG, "All modules initialized successfully");
    
    // Create main task (20KB stack for HTTPS operations)
    xTaskCreate(app_task, "main_logic", 20480, NULL, 5, NULL);

        // Create voice task (12KB stack for audio + HTTP)
    xTaskCreate(voice_task, "voice_logic", 12288, NULL, 4, NULL);
}