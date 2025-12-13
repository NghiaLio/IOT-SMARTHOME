#include <stdio.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "wifi_manager.h"
#include "led.h"

#include "esp_netif.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include <stdlib.h>
#include <ctype.h>

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_board_init.h"
#include "model_path.h"

// Firebase
#define FIREBASE_URL          "https://smart-944cb-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_LED_PATH     "devices/esp123/data/ledState.json"
// LED pin
#define LED_GPIO       2

int ledState = 0;

static const char *TAG = "FIREBASE_DEMO";
// ================= HTTP GET với Event Handler =================
static char g_led_body[32];  // Buffer static để lưu response body
static size_t g_received_len = 0;

// ================= HTTP PUT =================
static void http_put_json_at(const char* path, const char* json)
{
    char url[256];
    snprintf(url, sizeof(url), "%s%s", FIREBASE_URL, path);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_PUT,
        .skip_cert_common_name_check = true, // chỉ test
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client for %s", path);
        return;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, strlen(json));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    if (err == ESP_OK && (status >= 200 && status < 300)) {
        ESP_LOGI(TAG, "PUT %s -> Status=%d", path, status);
    } else {
        ESP_LOGE(TAG, "PUT %s Failed: err=%d (Status=%d)", path, (int)err, status);
    }
    esp_http_client_cleanup(client);
}


static void http_put_led_state(int state)
{
    char json[8];
    snprintf(json, sizeof(json), "%d", state ? 1 : 0);
    http_put_json_at(FIREBASE_LED_PATH, json);
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (g_received_len + evt->data_len > sizeof(g_led_body) - 1) {
                ESP_LOGW(TAG, "Buffer overflow, truncating");
                evt->data_len = sizeof(g_led_body) - 1 - g_received_len;
            }
            if (evt->data_len > 0) {
                memcpy(g_led_body + g_received_len, evt->data, evt->data_len);
                g_received_len += evt->data_len;
                g_led_body[g_received_len] = '\0';
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH, received len=%d", g_received_len);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static int http_get_led_state(void)
{
    // Reset buffer
    memset(g_led_body, 0, sizeof(g_led_body));
    g_received_len = 0;

    char url[128];
    snprintf(url, sizeof(url), "%s%s", FIREBASE_URL, FIREBASE_LED_PATH);
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .skip_cert_common_name_check = true,
        .event_handler = http_event_handler,  // Thêm event handler
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return -1;
    ESP_LOGI(TAG, "HTTP GET URL: %s", url);

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GET perform failed: err=%d", (int)err);
        esp_http_client_cleanup(client);
        return -1;
    }

    int status = esp_http_client_get_status_code(client);
    if (status < 200 || status >= 300) {
        ESP_LOGE(TAG, "GET invalid status: %d", status);
        esp_http_client_cleanup(client);
        return -1;
    }

    if (g_received_len <= 0) {
        ESP_LOGW(TAG, "Empty LED body (received=%d)", g_received_len);
        esp_http_client_cleanup(client);
        return -1;
    }
    ESP_LOGI(TAG, "LED raw body: '%s'", g_led_body);

    // Trim whitespace & optional quotes
    char *p = g_led_body;
    while (*p && (isspace((unsigned char)*p) || *p == '"')) p++;
    char *end = p;
    while (*end && isdigit((unsigned char)*end)) end++;
    *end = '\0';  // Null-terminate sau digit
    int led = (int)strtol(p, NULL, 10);
    if (led != 0 && led != 1) {  // Validate LED state
        ESP_LOGW(TAG, "Invalid LED value: %d", led);
        led = -1;
    }

    esp_http_client_cleanup(client);
    return led;
}


int detect_flag = 0;
static const esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;

void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_feed_channel_num(afe_data);
    int feed_channel = esp_get_feed_channel();
    assert(nch==feed_channel);
    int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
    assert(i2s_buff);

    while (task_flag) {
        esp_get_feed_data(true, i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);

        afe_handle->feed(afe_data, i2s_buff);
    }
    if (i2s_buff) {
        free(i2s_buff);
        i2s_buff = NULL;
    }
    vTaskDelete(NULL);
}

void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    int16_t *buff = malloc(afe_chunksize * sizeof(int16_t));
    assert(buff);
    printf("------------detect start------------\n");

    // modify wakenet detection threshold
    afe_handle->set_wakenet_threshold(afe_data, 1, 0.6); // set model1's threshold to 0.6
    afe_handle->set_wakenet_threshold(afe_data, 2, 0.6); // set model2's threshold to 0.6
    afe_handle->reset_wakenet_threshold(afe_data, 1);    // reset model1's threshold to default
    afe_handle->reset_wakenet_threshold(afe_data, 2);    // reset model2's threshold to default

    while (task_flag) {
        afe_fetch_result_t* res = afe_handle->fetch(afe_data); 
        if (!res || res->ret_value == ESP_FAIL) {
            printf("fetch error!\n");
            break;
        }
        // printf("vad state: %d\n", res->vad_state);

        if (res->wakeup_state == WAKENET_DETECTED) {
            printf("wakeword detected\n");
	        printf("model index:%d, word index:%d\n", res->wakenet_model_index, res->wake_word_index);
            printf("-----------LISTENING-----------\n");
        }
    }
    if (buff) {
        free(buff);
        buff = NULL;
    }
    vTaskDelete(NULL);
}

void start_wakenet()
{
    ESP_ERROR_CHECK(esp_board_init(16000, 1, 16));
    // ESP_ERROR_CHECK(esp_sdcard_init("/sdcard", 10));

    srmodel_list_t *models = esp_srmodel_init("model");
    if (models) {
        for (int i=0; i<models->num; i++) {
            if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL) {
                printf("wakenet model in flash: %s\n", models->model_name[i]);
            }
        }
    }

    afe_config_t *afe_config = afe_config_init(esp_get_input_format(), models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    
    // print/modify wake word model. 
    if (afe_config->wakenet_model_name) {
        printf("wakeword model in AFE config: %s\n", afe_config->wakenet_model_name);
    }
    if (afe_config->wakenet_model_name_2) {
        printf("wakeword model in AFE config: %s\n", afe_config->wakenet_model_name_2);
    }

    afe_handle = esp_afe_handle_from_config(afe_config);
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    

    // 
    afe_config_free(afe_config);
    
    task_flag = 1;
    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void*)afe_data, 5, NULL, 0);
    xTaskCreatePinnedToCore(&detect_Task, "detect", 4 * 1024, (void*)afe_data, 5, NULL, 1);
}

volatile bool wifi_connected = false;

// Hàm callback cập nhật trạng thái kết nối Wi-Fi
void wifi_manager_on_connected(void) {
    wifi_connected = true;
}

void app_main(void)
{
    // Init NVS
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(nvs_flash_init());
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    // Start WakeNet
    start_wakenet();

    // Start Wi-Fi manager
    wifi_manager_start();

    // Chờ đến khi kết nối Wi-Fi thành công
    while (!wifi_connected) {
        //ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "Wi-Fi connected, starting main loop");

    while (1)
    {
        // 2️⃣ GET từ Firebase
        int fbLed = http_get_led_state();
        ESP_LOGI(TAG, "LED state from Firebase: %d", fbLed);
        if (fbLed >= 0) {
            if (fbLed != ledState) {  // Chỉ cập nhật nếu thay đổi
                ledState = fbLed;
                gpio_set_level(LED_GPIO, ledState);
                ESP_LOGI(TAG, "LED updated to: %d", ledState);
                // Optional: PUT back để sync (nếu cần)
                http_put_led_state(ledState);
            }
        }

        // // 3️⃣ PUT dữ liệu cảm biến (tách biệt, cập nhật thường xuyên)
        // sensors.temperature = temperature;
        // sensors.humidity = humidity;
        // http_put_sensors(&sensors);

        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Giảm delay xuống 1 giây để phản hồi nhanh hơn
    }
}
