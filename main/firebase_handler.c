#include "firebase_handler.h"
#include "config.h"
#include "wifi_handler.h"
#include "actuators.h"
#include "rfid_handler.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "FIREBASE";

// For Firebase GET
static char g_firebase_body[FIREBASE_BUFFER_SIZE];
static size_t g_firebase_len = 0;

static esp_err_t firebase_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (g_firebase_len + evt->data_len < sizeof(g_firebase_body) - 1) {
                memcpy(g_firebase_body + g_firebase_len, evt->data, evt->data_len);
                g_firebase_len += evt->data_len;
                g_firebase_body[g_firebase_len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

char* send_to_firebase(char *json_data, esp_http_client_method_t method, char *path) {
    if (!wifi_connected) {
        ESP_LOGE(TAG, "Chua co Wifi!");
        return NULL;
    }

    ESP_LOGI(TAG, "Sending to Firebase (%s): %s", method == HTTP_METHOD_PATCH ? "PATCH" : "POST", json_data);

    char url[256];
    sprintf(url, "https://smart-944cb-default-rtdb.asia-southeast1.firebasedatabase.app/devices/%s/%s", FIREBASE_DEVICE_ID, path);

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = true,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return NULL;
    }

    esp_http_client_set_method(client, method);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data, strlen(json_data));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (err == ESP_OK && (status >= 200 && status < 300)) {
        ESP_LOGI(TAG, "Firebase %s status: %d", method == HTTP_METHOD_PATCH ? "PATCH" : "POST", status);
    } else {
        ESP_LOGE(TAG, "Firebase %s failed: err=%d (Status=%d)", method == HTTP_METHOD_PATCH ? "PATCH" : "POST", (int)err, status);
    }

    esp_http_client_cleanup(client);
    return NULL;
}

void get_firebase_data(void) {
    if (!wifi_connected) return;
    
    // Reset buffer
    memset(g_firebase_body, 0, sizeof(g_firebase_body));
    g_firebase_len = 0;

    esp_http_client_config_t config = {
        .url = "https://smart-944cb-default-rtdb.asia-southeast1.firebasedatabase.app/devices/esp123/data.json",
        .method = HTTP_METHOD_GET,
        .event_handler = firebase_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init client for GET");
        return;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err == ESP_OK && status == 200 && g_firebase_len > 0) {
        ESP_LOGI(TAG, "Firebase response: %s", g_firebase_body);
        cJSON *root = cJSON_Parse(g_firebase_body);
        if (root) {
            cJSON *led = cJSON_GetObjectItem(root, "ledState");
            if (cJSON_IsNumber(led)) {
                int new_led = led->valueint;
                if (new_led != led_state) {
                    gpio_set_level(LED_PIN, new_led);
                    led_state = new_led;
                    ESP_LOGI(TAG, "Firebase LED updated: %d", led_state);
                }
            } else {
                ESP_LOGW(TAG, "ledState not found or not number");
            }
            
            cJSON *fan = cJSON_GetObjectItem(root, "fanState");
            if (cJSON_IsNumber(fan)) {
                int new_fan = fan->valueint;
                if (new_fan != fan_state) {
                    gpio_set_level(RELAY_FAN_PIN, !new_fan);
                    fan_state = new_fan;
                    ESP_LOGI(TAG, "Firebase Fan updated: %d", fan_state);
                }
            } else {
                ESP_LOGW(TAG, "fanState not found or not number");
            }
            
            cJSON *ac = cJSON_GetObjectItem(root, "acState");
            if (cJSON_IsNumber(ac)) {
                int new_ac = ac->valueint;
                if (new_ac != ac_state) {
                    gpio_set_level(RELAY_AC_PIN, !new_ac);
                    ac_state = new_ac;
                    ESP_LOGI(TAG, "Firebase AC updated: %d", ac_state);
                }
            } else {
                ESP_LOGW(TAG, "acState not found or not number");
            }
            
            cJSON *door = cJSON_GetObjectItem(root, "doorAngle");
            if (cJSON_IsNumber(door)) {
                int new_angle = door->valueint;
                if (new_angle != door_angle) {
                    set_servo_angle(new_angle);
                    door_angle = new_angle;
                    ESP_LOGI(TAG, "Firebase Door updated: %d", door_angle);
                }
            } else {
                ESP_LOGW(TAG, "doorAngle not found or not number");
            }
            
            cJSON *rain = cJSON_GetObjectItem(root, "rainAngle");
            if (cJSON_IsNumber(rain)) {
                int new_rain_angle = rain->valueint;
                if (new_rain_angle != rain_angle) {
                    set_rain_servo_angle(new_rain_angle);
                    rain_angle = new_rain_angle;
                    ESP_LOGI(TAG, "Firebase Rain Servo updated: %d", rain_angle);
                }
            } else {
                ESP_LOGW(TAG, "rainAngle not found or not number");
            }
            
            cJSON *add = cJSON_GetObjectItem(root, "addCard");
            if (cJSON_IsNumber(add)) {
                add_card = add->valueint;
                ESP_LOGI(TAG, "Firebase addCard updated: %d", add_card);
            } else {
                ESP_LOGW(TAG, "addCard not found or not number");
            }
            
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG, "Failed to parse Firebase JSON");
        }
    } else {
        ESP_LOGW(TAG, "Firebase GET failed: err=%d, status=%d, len=%d", (int)err, status, (int)g_firebase_len);
    }
}
