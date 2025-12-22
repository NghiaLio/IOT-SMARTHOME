#include "voice_handler.h"
#include "config.h"
#include "wifi_handler.h"
#include "firebase_handler.h"
#include "actuators.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "VOICE";

void parse_wit_response(char *json_str) {
    // Parse chuỗi JSON
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON loi hoac rong!");
        return;
    }

    // Tìm trường "text"
    cJSON *text_item = cJSON_GetObjectItem(root, "text");
    char *cmd = NULL;
    if (cJSON_IsString(text_item) && (text_item->valuestring != NULL)) {
        cmd = strdup(text_item->valuestring);
    }

    // Xóa JSON để giải phóng RAM trước khi gửi Firebase
    cJSON_Delete(root);

    if (cmd) {
        ESP_LOGW(TAG, "Nhan dien duoc: '%s'", cmd);

        if (strlen(cmd) == 0) {
            ESP_LOGW(TAG, "Khong nghe ro lenh (Empty)");
        }
        // Logic so sánh từ khóa
        else if ((strstr(cmd, "Bật") || strstr(cmd, "bật") || strstr(cmd, "Mở") || strstr(cmd, "mở")) && (strstr(cmd, "đèn") || strstr(cmd, "den"))) {
            gpio_set_level(LED_PIN, 1);
            led_state = 1;
            ESP_LOGI(TAG, "--> THUC THI: BAT DEN (ON)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d,\"acState\":%d}", led_state, fan_state, door_angle, ac_state);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        } 
        else if ((strstr(cmd, "Tắt") || strstr(cmd, "tắt")) && (strstr(cmd, "đèn") || strstr(cmd, "den"))) {
            gpio_set_level(LED_PIN, 0);
            led_state = 0;
            ESP_LOGI(TAG, "--> THUC THI: TAT DEN (OFF)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d,\"acState\":%d}", led_state, fan_state, door_angle, ac_state);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        } 
        else if ((strstr(cmd, "Bật") || strstr(cmd, "bật") || strstr(cmd, "Mở") || strstr(cmd, "mở")) && strstr(cmd, "quạt")) {
            gpio_set_level(RELAY_FAN_PIN, 0);
            fan_state = 1;
            ESP_LOGI(TAG, "--> THUC THI: BAT QUAT (ON)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d,\"acState\":%d}", led_state, fan_state, door_angle, ac_state);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        }
        else if ((strstr(cmd, "Tắt") || strstr(cmd, "tắt")) && strstr(cmd, "quạt")) {
            gpio_set_level(RELAY_FAN_PIN, 1);
            fan_state = 0;
            ESP_LOGI(TAG, "--> THUC THI: TAT QUAT (OFF)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d,\"acState\":%d}", led_state, fan_state, door_angle, ac_state);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        } 
        else if ((strstr(cmd, "Bật") || strstr(cmd, "bật") || strstr(cmd, "Mở") || strstr(cmd, "mở")) && (strstr(cmd, "điều hòa") || strstr(cmd, "dieu hoa"))) {
            gpio_set_level(RELAY_AC_PIN, 0);
            ac_state = 1;
            ESP_LOGI(TAG, "--> THUC THI: BAT DIEU HOA (ON)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d,\"acState\":%d}", led_state, fan_state, door_angle, ac_state);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        }
        else if ((strstr(cmd, "Tắt") || strstr(cmd, "tắt") || strstr(cmd, "Đóng") || strstr(cmd, "đóng")) && (strstr(cmd, "điều hòa") || strstr(cmd, "dieu hoa"))) {
            gpio_set_level(RELAY_AC_PIN, 1);
            ac_state = 0;
            ESP_LOGI(TAG, "--> THUC THI: TAT DIEU HOA (OFF)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d,\"acState\":%d}", led_state, fan_state, door_angle, ac_state);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        } 
        else if ((strstr(cmd, "Mở") || strstr(cmd, "mở")) && strstr(cmd, "cửa")) {
            set_servo_angle(90);
            door_angle = 90;
            ESP_LOGI(TAG, "--> THUC THI: MO CUA SERVO (90deg)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d,\"acState\":%d}", led_state, fan_state, door_angle, ac_state);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        } 
        else if ((strstr(cmd, "Đóng") || strstr(cmd, "đóng")) && strstr(cmd, "cửa")) {
            set_servo_angle(0);
            door_angle = 0;
            ESP_LOGI(TAG, "--> THUC THI: DONG CUA SERVO (0deg)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d,\"acState\":%d}", led_state, fan_state, door_angle, ac_state);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        } 
        else if ((strstr(cmd, "Mở") || strstr(cmd, "mở")) && (strstr(cmd, "mái che") || strstr(cmd, "mai che"))) {
            set_rain_servo_angle(90);
            rain_angle = 90;
            ESP_LOGI(TAG, "--> THUC THI: MO MAI CHE (90deg)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d,\"acState\":%d,\"rainAngle\":%d}", led_state, fan_state, door_angle, ac_state, rain_angle);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        }
        else if ((strstr(cmd, "Đóng") || strstr(cmd, "đóng")) && (strstr(cmd, "mái che") || strstr(cmd, "mai che"))) {
            set_rain_servo_angle(0);
            rain_angle = 0;
            ESP_LOGI(TAG, "--> THUC THI: DONG MAI CHE (0deg)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d,\"acState\":%d,\"rainAngle\":%d}", led_state, fan_state, door_angle, ac_state, rain_angle);
            send_to_firebase(json, HTTP_METHOD_PATCH, "data.json");
        }

        free(cmd);
    }
}

void send_to_wit_ai(char *audio_data, int len) {
    if (!wifi_connected) {
        ESP_LOGE(TAG, "Chua co Wifi!");
        return;
    }
    
    ESP_LOGI(TAG, "Dang gui len Wit.ai... (Free Heap: %d)", (int)esp_get_free_heap_size());

    esp_http_client_config_t config = {
        .url = "https://api.wit.ai/speech?v=20230215",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Authorization", "Bearer " WIT_ACCESS_TOKEN);
    esp_http_client_set_header(client, "Content-Type", "audio/raw;encoding=signed-integer;bits=16;rate=16000;endian=little");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    esp_err_t err = esp_http_client_open(client, len);
    if (err == ESP_OK) {
        int wlen = esp_http_client_write(client, audio_data, len);
        if (wlen < 0) ESP_LOGE(TAG, "Loi ghi audio");
        
        (void)esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Status: %d", status);

        if (status == 200) {
            int buffer_size = 2048;
            char *resp_buf = calloc(1, buffer_size + 1);
            if (resp_buf) {
                int read_len = esp_http_client_read_response(client, resp_buf, buffer_size);
                if (read_len > 0) {
                    resp_buf[read_len] = 0;
                    ESP_LOGI(TAG, "JSON: %s", resp_buf);
                    parse_wit_response(resp_buf);
                }
                free(resp_buf);
            }
        }
    } else {
        ESP_LOGE(TAG, "Loi Connect: %s", esp_err_to_name(err));
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}
