#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h" // Thư viện SSL cho HTTPS
#include "cJSON.h"
#include "rom/ets_sys.h"
#include "esp_adc/adc_oneshot.h"
#include "rc522.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"
#include <esp_timer.h>

static const char *TAG_2 = "RFID_APP";

// ============================================================
// 1. CẤU HÌNH NGƯỜI DÙNG (THAY ĐỔI TẠI ĐÂY)
// ============================================================
#define WIFI_SSID           "TP-Link_FA7F"
#define WIFI_PASS           "24681537"
#define WIT_ACCESS_TOKEN    "SNQYPCD3R67FQST7SMPDVYCE4GJNNV5V" // Ví dụ: 54SDF...

// Firebase config
#define FIREBASE_PROJECT_ID "smart-944cb"  // Thay bằng project ID thật
#define FIREBASE_DEVICE_ID  "esp123"
#define FIREBASE_API_KEY   "your-api-key"     // Nếu cần auth

// Cấu hình Chân & Mic
#define I2S_SCK_PIN         GPIO_NUM_11
#define I2S_WS_PIN          GPIO_NUM_12
#define I2S_SD_PIN          GPIO_NUM_10
#define I2S_MCLK_PIN        GPIO_NUM_13  // Thêm MCLK cho mic
#define LED_PIN             GPIO_NUM_2

// Định nghĩa chân kết nối cho RFID (đã đổi để tránh trùng)
#define RC522_SDA_GPIO  5   // CS
#define RC522_SCK_GPIO  21  // SCLK
#define RC522_MOSI_GPIO 20  // MOSI
#define RC522_MISO_GPIO 19  // MISO
#define RC522_RST_GPIO  -1  // RST (đặt -1 nếu không dùng)

// Cấu hình Relay
#define RELAY_FAN_PIN       GPIO_NUM_14

// Cấu hình Servo cho Đèn
#define SERVO_LIGHT_PIN     GPIO_NUM_8
#define SERVO_LIGHT_CHANNEL LEDC_CHANNEL_0
#define SERVO_LIGHT_TIMER   LEDC_TIMER_0
#define SERVO_MIN_PULSEWIDTH 500   // 0.5ms
#define SERVO_MAX_PULSEWIDTH 2500  // 2.5ms
#define SERVO_MAX_DEGREE     180   // Max angle

// Cấu hình DHT11
#define DHT_PIN             GPIO_NUM_15

// Cấu hình MQ2 Gas Sensor
#define GAS_PIN             GPIO_NUM_7  // Analog pin for MQ2
#define GAS_ADC_CHANNEL     ADC_CHANNEL_6  // ADC1_CH6

// Cấu hình Flame Sensor
#define FLAME_PIN           GPIO_NUM_16  // Digital pin for flame sensor

// Cấu hình Rain Sensor
#define RAIN_PIN            GPIO_NUM_17  // Digital pin for rain sensor

// Cấu hình Buzzer
#define BUZZER_PIN          GPIO_NUM_18  // Buzzer pin for alarm

// Cấu hình Ghi âm (Đã tối ưu để không tràn RAM)
#define SAMPLE_RATE         16000
#define REC_TIME_SEC        1.5      // Ghi âm 1.5 giây là đủ cho lệnh ngắn
#define RECORD_SIZE         (int)(SAMPLE_RATE * REC_TIME_SEC * 2) 
#define SOUND_THRESHOLD     3000     // Giảm ngưỡng để test phát hiện âm thanh

static const char *TAG = "SMART_VOICE_FINAL";
i2s_chan_handle_t rx_handle = NULL;
adc_oneshot_unit_handle_t adc1_handle;
bool wifi_connected = false;

// Timer for LED blink
esp_timer_handle_t led_blink_timer;

// Callback to turn off LED after blink
void led_blink_callback(void *arg) {
    gpio_set_level(LED_PIN, 0);
}

// Function to blink LED for 1 second
void blink_led() {
    gpio_set_level(LED_PIN, 1);
    esp_timer_start_once(led_blink_timer, 1000000); // 1 second
}

// Device states
int led_state = 0;
int fan_state = 0;
int door_angle = 0;

// For Firebase GET
static char g_firebase_body[512];
static size_t g_firebase_len = 0;

// Hàm xử lý sự kiện khi có thẻ quét qua
static rc522_driver_handle_t s_rc522_driver = NULL;
static rc522_handle_t s_rc522 = NULL;

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        char uid_str[RC522_PICC_UID_STR_BUFFER_SIZE_MAX] = {0};
        if (rc522_picc_uid_to_str(&picc->uid, uid_str, sizeof(uid_str)) == ESP_OK) {
            ESP_LOGI(TAG, "RFID: UID=%s, Type=%s", uid_str, rc522_picc_type_name(picc->type));
        } else {
            ESP_LOGI(TAG, "RFID: Card detected (failed to format UID)");
        }
        blink_led(); // Blink LED on card scan
    } else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "RFID: Card removed");
    }
}

static void init_rfid(){
    ESP_LOGI(TAG, "Khoi dong he thong RFID (SPI)...");

    rc522_spi_config_t driver_config = {
        .host_id = SPI3_HOST,
        .bus_config = &(spi_bus_config_t){
            .miso_io_num = RC522_MISO_GPIO,
            .mosi_io_num = RC522_MOSI_GPIO,
            .sclk_io_num = RC522_SCK_GPIO,
        },
        .dev_config = {
            .spics_io_num = RC522_SDA_GPIO,
        },
        .dma_chan = SPI_DMA_CH_AUTO,
        .rst_io_num = RC522_RST_GPIO, // dat -1 neu khong dung chan RST
    };

    esp_err_t err = rc522_spi_create(&driver_config, &s_rc522_driver);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rc522_spi_create failed: %s", esp_err_to_name(err));
        return;
    }

    err = rc522_driver_install(s_rc522_driver);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rc522_driver_install failed: %s", esp_err_to_name(err));
        return;
    }

    rc522_config_t scanner_config = {
        .driver = s_rc522_driver,
    };

    err = rc522_create(&scanner_config, &s_rc522);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rc522_create failed: %s", esp_err_to_name(err));
        return;
    }

    rc522_register_events(s_rc522, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL);

    err = rc522_start(s_rc522);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rc522_start failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "RFID san sang. Hay dua the vao de quet.");
}

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
float temperature = 0.0;
float humidity = 0.0;
float gas_level = 0.0;
int flame_detected = 0;
int rain_detected = 0;

// ============================================================
// 2. HÀM XỬ LÝ WIFI (BOILERPLATE)
// ============================================================
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        esp_wifi_connect();
        ESP_LOGW(TAG, "Mat Wifi, dang ket noi lai...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ESP_LOGI(TAG, ">> WIFI CONNECTED! <<");
        blink_led(); // Blink LED on WiFi connect
    }
}
void wifi_init_sta(void) {
    nvs_flash_init(); esp_netif_init(); esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS, .threshold.authmode = WIFI_AUTH_WPA2_PSK } };
    esp_wifi_set_mode(WIFI_MODE_STA); esp_wifi_set_config(WIFI_IF_STA, &wifi_config); esp_wifi_start();
}

// ============================================================
// 3. HÀM XỬ LÝ JSON & ĐIỀU KHIỂN
// ============================================================
void set_servo_angle(int angle); // Prototype
char* send_to_firebase(char *json_data, esp_http_client_method_t method); // Prototype
void get_firebase_data(void); // Prototype

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
        cmd = strdup(text_item->valuestring); // Sao chép để giữ sau khi xóa JSON
    }

    // [QUAN TRỌNG] Xóa JSON để giải phóng RAM trước khi gửi Firebase
    cJSON_Delete(root);

    if (cmd) {
        ESP_LOGW(TAG, "Nhan dien duoc: '%s'", cmd);

        if (strlen(cmd) == 0) {
            ESP_LOGW(TAG, "Khong nghe ro lenh (Empty)");
        }
        // Logic so sánh từ khóa (Linh hoạt)
        else if ((strstr(cmd, "Bật") || strstr(cmd, "bật") || strstr(cmd, "Mở") || strstr(cmd, "mở")) && (strstr(cmd, "đèn") || strstr(cmd, "den"))) {
            gpio_set_level(LED_PIN, 1);
            led_state = 1;
            ESP_LOGI(TAG, "--> THUC THI: BAT DEN (ON)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d}", led_state, fan_state, door_angle);
            send_to_firebase(json, HTTP_METHOD_PATCH);
        } 
        else if ((strstr(cmd, "Tắt") || strstr(cmd, "tắt")) && (strstr(cmd, "đèn") || strstr(cmd, "den"))) {
            gpio_set_level(LED_PIN, 0);
            led_state = 0;
            ESP_LOGI(TAG, "--> THUC THI: TAT DEN (OFF)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d}", led_state, fan_state, door_angle);
            send_to_firebase(json, HTTP_METHOD_PATCH);
        } 
        else if ((strstr(cmd, "Bật") || strstr(cmd, "bật") || strstr(cmd, "Mở") || strstr(cmd, "mở")) && strstr(cmd, "quạt")) {
            gpio_set_level(RELAY_FAN_PIN, 1);
            fan_state = 1;
            ESP_LOGI(TAG, "--> THUC THI: BAT QUAT (ON)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d}", led_state, fan_state, door_angle);
            send_to_firebase(json, HTTP_METHOD_PATCH);
        }
        else if ((strstr(cmd, "Tắt") || strstr(cmd, "tắt")) && strstr(cmd, "quạt")) {
            gpio_set_level(RELAY_FAN_PIN, 0);
            fan_state = 0;
            ESP_LOGI(TAG, "--> THUC THI: TAT QUAT (OFF)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d}", led_state, fan_state, door_angle);
            send_to_firebase(json, HTTP_METHOD_PATCH);
        } 
        else if ((strstr(cmd, "Mở") || strstr(cmd, "mở")) && strstr(cmd, "cửa")) {
            set_servo_angle(90); // Mở servo
            door_angle = 90;
            ESP_LOGI(TAG, "--> THUC THI: MO CUA SERVO (90deg)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d}", led_state, fan_state, door_angle);
            send_to_firebase(json, HTTP_METHOD_PATCH);
        } 
        else if ((strstr(cmd, "Đóng") || strstr(cmd, "đóng")) && strstr(cmd, "cửa")) {
            set_servo_angle(0); // Đóng servo
            door_angle = 0;
            ESP_LOGI(TAG, "--> THUC THI: DONG CUA SERVO (0deg)");
            char json[128];
            sprintf(json, "{\"ledState\":%d,\"fanState\":%d,\"doorAngle\":%d}", led_state, fan_state, door_angle);
            send_to_firebase(json, HTTP_METHOD_PATCH);
        }

        free(cmd); // Giải phóng chuỗi cmd
    }
}

// ============================================================
// 4.5. HÀM GỬI DỮ LIỆU LÊN FIREBASE
// ============================================================
char* send_to_firebase(char *json_data, esp_http_client_method_t method) {
    if (!wifi_connected) { ESP_LOGE(TAG, "Chua co Wifi!"); return NULL; }

    ESP_LOGI(TAG, "Sending to Firebase (PATCH): %s", json_data);

    char url[256];
    sprintf(url, "https://smart-944cb-default-rtdb.asia-southeast1.firebasedatabase.app/devices/%s/data.json", FIREBASE_DEVICE_ID);

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = true, // chỉ test
        .timeout_ms = 10000, // Tăng timeout lên 10s
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
        ESP_LOGI(TAG, "Firebase PATCH status: %d", status);
    } else {
        ESP_LOGE(TAG, "Firebase PATCH failed: err=%d (Status=%d)", (int)err, status);
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
                    gpio_set_level(RELAY_FAN_PIN, new_fan);
                    fan_state = new_fan;
                    ESP_LOGI(TAG, "Firebase Fan updated: %d", fan_state);
                }
            } else {
                ESP_LOGW(TAG, "fanState not found or not number");
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
            
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG, "Failed to parse Firebase JSON");
        }
    } else {
        ESP_LOGW(TAG, "Firebase GET failed: err=%d, status=%d, len=%d", (int)err, status, (int)g_firebase_len);
    }
}

void send_to_wit_ai(char *audio_data, int len) {
    if (!wifi_connected) { ESP_LOGE(TAG, "Chua co Wifi!"); return; }
    
    // Kiểm tra RAM trước khi gửi
    ESP_LOGI(TAG, "Dang gui len Wit.ai... (Free Heap: %d)", (int)esp_get_free_heap_size());

    esp_http_client_config_t config = {
        .url = "https://api.wit.ai/speech?v=20230215",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
        .crt_bundle_attach = esp_crt_bundle_attach, // SSL
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Cài đặt Header
    esp_http_client_set_header(client, "Authorization", "Bearer " WIT_ACCESS_TOKEN);
    esp_http_client_set_header(client, "Content-Type", "audio/raw;encoding=signed-integer;bits=16;rate=16000;endian=little");
    esp_http_client_set_header(client, "Accept-Encoding", "identity"); // Không nén

    // Mở kết nối
    esp_err_t err = esp_http_client_open(client, len);
    if (err == ESP_OK) {
        // Ghi dữ liệu Audio
        int wlen = esp_http_client_write(client, audio_data, len);
        if (wlen < 0) ESP_LOGE(TAG, "Loi ghi audio");
        
        // Lấy phản hồi
        (void)esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Status: %d", status);

        if (status == 200) {
            // Đọc Body trả về
            int buffer_size = 2048; // Đủ cho JSON ngắn
            char *resp_buf = calloc(1, buffer_size + 1);
            if (resp_buf) {
                int read_len = esp_http_client_read_response(client, resp_buf, buffer_size);
                if (read_len > 0) {
                    resp_buf[read_len] = 0; // Kết thúc chuỗi
                    ESP_LOGI(TAG, "JSON: %s", resp_buf);
                    parse_wit_response(resp_buf); // Xử lý
                }
                free(resp_buf); // Giải phóng ngay
            }
        }
    } else {
        ESP_LOGE(TAG, "Loi Connect: %s", esp_err_to_name(err));
    }
    
    // Đóng và dọn dẹp
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

// ============================================================
// DHT11 READ FUNCTION
// ============================================================
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
        if(timeout-- <= 0) return ESP_ERR_TIMEOUT; // Hết giờ -> Thoát ngay
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

    // 3. Đọc 40 bit dữ liệu (QUAN TRỌNG: THÊM TIMEOUT VÀO ĐÂY)
    for (int i = 0; i < 40; i++) {
        // Chờ bit bắt đầu (Mức 0)
        timeout = 1000; 
        while(gpio_get_level(pin) == 0) {
            if(timeout-- <= 0) return ESP_ERR_TIMEOUT; // Treo ở mức 0 -> Thoát
            ets_delay_us(1);
        }
        
        // Đo độ rộng xung mức 1
        ets_delay_us(30); 
        if (gpio_get_level(pin)) {
            data[i/8] |= (1 << (7 - (i%8))); // Nếu sau 30us vẫn là 1 -> Bit 1
        }
        
        // Chờ bit kết thúc (Mức 1 xuống 0)
        timeout = 1000;
        while(gpio_get_level(pin) == 1) {
            if(timeout-- <= 0) return ESP_ERR_TIMEOUT; // Treo ở mức 1 -> Thoát
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

// ============================================================
// 5. TASK CHÍNH
// ============================================================
void app_task(void *args) {
    // Khởi động trễ để ổn định điện áp
    ESP_LOGI(TAG, "Khoi dong he thong... (Cho 3s)");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "SAN SANG! Hay noi 'Bat den'...");

    // Buffer tạm để đọc mẫu kiểm tra độ ồn
    int16_t temp_buf[256];
    size_t r_bytes = 0;

    // Xả rác Mic lần đầu
    for(int i=0; i<20; i++) i2s_channel_read(rx_handle, temp_buf, sizeof(temp_buf), &r_bytes, 10);

    // static int sensor_counter = 0;
    static int dht_read_counter = 0;
    static float prev_temperature = -1.0;
    static float prev_humidity = -1.0;
    static float prev_gas_level = -1.0;
    static int sensor_counter = 0;
    static int firebase_get_counter = 0;
    static int prev_flame_detected = -1;
    static int prev_rain_detected = -1;
    static int flame_debounce = 0;
    static int rain_debounce = 0;

    while (1) {
        // Đọc DHT11 mỗi 1 giây (để tránh chậm vòng lặp)
        dht_read_counter++;
        if (dht_read_counter >= 50) { // 50 * 20ms = 1s
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
        
        // Đọc MQ2 Gas Sensor liên tục
        int gas_raw = 0;
        adc_oneshot_read(adc1_handle, GAS_ADC_CHANNEL, &gas_raw);
        gas_level = gas_raw;
        if (gas_level > 1500 && gas_level != prev_gas_level) {
            ESP_LOGW(TAG, "MQ2 Gas Level HIGH: %.0f", gas_level);
            prev_gas_level = gas_level;
            // Gửi ngay lên Firebase khi vượt ngưỡng
            char json[64];
            sprintf(json, "{\"gasLevel\":%.0f}", gas_level);
            send_to_firebase(json, HTTP_METHOD_PATCH);
        }
        
        // Đọc Flame Sensor liên tục
        int current_flame = !gpio_get_level(FLAME_PIN);
        if (current_flame == flame_detected) {
            flame_debounce++;
            if (flame_debounce >= 5) { // Stable for 5 loops (100ms)
                if (flame_detected != prev_flame_detected) {
                    ESP_LOGW(TAG, "Flame Detected: %d", flame_detected);
                    prev_flame_detected = flame_detected;
                    // Bật/Tắt còi báo động
                    gpio_set_level(BUZZER_PIN, flame_detected);
                    ESP_LOGI(TAG, "Buzzer State: %d", flame_detected);
                    // Gửi ngay lên Firebase khi có thay đổi
                    char json[64];
                    sprintf(json, "{\"flameDetected\":%d}", flame_detected);
                    send_to_firebase(json, HTTP_METHOD_PATCH);
                }
                flame_debounce = 0;
            }
        } else {
            flame_detected = current_flame;
            flame_debounce = 0;
        }
        
        // Đọc Rain Sensor liên tục
        int current_rain = !gpio_get_level(RAIN_PIN);
        if (current_rain == rain_detected) {
            rain_debounce++;
            if (rain_debounce >= 5) { // Stable for 5 loops (100ms)
                if (rain_detected != prev_rain_detected) {
                    ESP_LOGW(TAG, "Rain Detected: %d", rain_detected);
                    prev_rain_detected = rain_detected;
                    // Gửi ngay lên Firebase khi có thay đổi
                    char json[64];
                    sprintf(json, "{\"rainDetected\":%d}", rain_detected);
                    send_to_firebase(json, HTTP_METHOD_PATCH);
                }
                rain_debounce = 0;
            }
        } else {
            rain_detected = current_rain;
            rain_debounce = 0;
        }
        
        // Nhận dữ liệu từ Firebase mỗi 1 giây (realtime)
        firebase_get_counter++;
        if (firebase_get_counter >= 50) { // 50 * 20ms = 1s
            firebase_get_counter = 0;
            get_firebase_data();
        }
        
        // Gửi lên Firebase mỗi 10 giây
        sensor_counter++;
        if (sensor_counter >= 500) { // 500 * 20ms = 10s
            sensor_counter = 0;
            char json[128];
            sprintf(json, "{\"temperature\":%.1f,\"humidity\":%.1f,\"gasLevel\":%.0f}", temperature, humidity, gas_level);
            send_to_firebase(json, HTTP_METHOD_PATCH);
        }

        // 1. Đọc mẫu nhỏ để check Volume
        i2s_channel_read(rx_handle, temp_buf, sizeof(temp_buf), &r_bytes, 100);
        // Tính volume từ mẫu
        int vol = 0;
        for(int i = 0; i < 256; i++) {
            int abs_val = abs(temp_buf[i]);
            if(abs_val > vol) vol = abs_val;
        }
        // 2. Nếu phát hiện tiếng nói lớn
        if (vol > SOUND_THRESHOLD && wifi_connected) {
            ESP_LOGI(TAG, ">>> PHAT HIEN TIENG NOI (Vol: %d) <<<", vol);
            
            // 3. Cấp phát RAM cho ghi âm (MALLOC)
            char *rec_buffer = (char *)calloc(1, RECORD_SIZE);
            if (rec_buffer == NULL) {
                ESP_LOGE(TAG, "Het RAM! Khong the ghi am.");
            } 
            else {
                // 4. Ghi âm 1.5 giây
                size_t bytes_recorded = 0;
                i2s_channel_read(rx_handle, rec_buffer, RECORD_SIZE, &bytes_recorded, portMAX_DELAY);
                
                // 5. Gửi đi xử lý
                send_to_wit_ai(rec_buffer, bytes_recorded);
                
                // 6. Giải phóng RAM ngay lập tức (FREE)
                free(rec_buffer);
                ESP_LOGI(TAG, "Da giai phong RAM. Cho lenh moi...");
            }

            // Xả buffer Mic để tránh lặp lại do tiếng vang
            for(int i=0; i<10; i++) i2s_channel_read(rx_handle, temp_buf, sizeof(temp_buf), &r_bytes, 10);
            vTaskDelay(pdMS_TO_TICKS(500)); 
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ============================================================
// 6. INIT PHẦN CỨNG
// ============================================================
void init_microphone(void) {
    // Cấu hình pull-up cho WS nếu cần
    gpio_set_pull_mode(I2S_WS_PIN, GPIO_PULLUP_ONLY);

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = { 
            .bclk = I2S_SCK_PIN, 
            .ws = I2S_WS_PIN, 
            .din = I2S_SD_PIN, 
            .mclk = I2S_MCLK_PIN,  // Sử dụng MCLK
            .dout = I2S_GPIO_UNUSED 
        },
    };
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    i2s_channel_init_std_mode(rx_handle, &std_cfg); 
    i2s_channel_enable(rx_handle);
}
void init_led() { 
    gpio_reset_pin(LED_PIN); 
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT); 
    gpio_reset_pin(RELAY_FAN_PIN); 
    gpio_set_direction(RELAY_FAN_PIN, GPIO_MODE_OUTPUT);
    gpio_config_t flame_config = {
        .pin_bit_mask = (1ULL << FLAME_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&flame_config);

    gpio_config_t rain_config = {
        .pin_bit_mask = (1ULL << RAIN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&rain_config);
    // gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT); // Buzzer output
    // gpio_set_level(BUZZER_PIN, 0); // Tắt còi ban đầu
}

void init_servo() {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = SERVO_LIGHT_TIMER,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num = SERVO_LIGHT_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = SERVO_LIGHT_CHANNEL,
        .timer_sel = SERVO_LIGHT_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel_conf);
}

void set_servo_angle(int angle) {
    uint32_t duty = (SERVO_MIN_PULSEWIDTH + (SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * angle / SERVO_MAX_DEGREE) * (1 << LEDC_TIMER_13_BIT) / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_LIGHT_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_LIGHT_CHANNEL);
}

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

void app_main(void) {
    init_rfid();
    init_led();
    // Init LED blink timer
    esp_timer_create_args_t timer_args = {
        .callback = led_blink_callback,
        .name = "led_blink"
    };
    esp_timer_create(&timer_args, &led_blink_timer);
    init_servo();
    init_adc();
    wifi_init_sta();
    init_microphone();
    ESP_LOGI(TAG, "Before init_rfid");
    // init_rfid();
    ESP_LOGI(TAG, "After init_rfid");
    
    // Tăng Stack lên 20KB (20480 bytes) để tránh lỗi Stack Overflow khi chạy HTTPS
    xTaskCreate(app_task, "main_logic", 20480, NULL, 5, NULL);
}