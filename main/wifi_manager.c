// Hàm callback để báo cho main.c khi kết nối Wi-Fi thành công
extern void wifi_manager_on_connected(void);
#include "wifi_manager.h"
#include "html_page.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"

static const char *TAG = "WiFiManager";
static httpd_handle_t server = NULL;
static int retry_count = 0;
#define MAX_RETRY 5

/* ===================== NVS ===================== */
static void save_wifi_config(const char *ssid, const char *pass){
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("wifi_config", NVS_READWRITE, &nvs));
    nvs_set_str(nvs,"ssid",ssid);
    nvs_set_str(nvs,"pass",pass);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG,"Wi-Fi config saved: SSID=%s",ssid);
}

static void save_device_token(const char *token){
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("wifi_config", NVS_READWRITE, &nvs));
    nvs_set_str(nvs,"token",token);
    nvs_commit(nvs);
    nvs_close(nvs);
}

static bool load_wifi_config(char *ssid, char *pass){
    nvs_handle_t nvs;
    if(nvs_open("wifi_config", NVS_READONLY, &nvs)!=ESP_OK) return false;
    size_t ssid_len=32, pass_len=64;
    if(nvs_get_str(nvs,"ssid",ssid,&ssid_len)!=ESP_OK) return false;
    if(nvs_get_str(nvs,"pass",pass,&pass_len)!=ESP_OK) return false;
    nvs_close(nvs);
    ESP_LOGI(TAG,"Wi-Fi config loaded: SSID=%s",ssid);
    return true;
}

/* ===================== HTTP HANDLER ===================== */
static esp_err_t root_get_handler(httpd_req_t *req){
    httpd_resp_set_type(req,"text/html");
    httpd_resp_send(req,HTML_PAGE,HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t wifi_post_handler(httpd_req_t *req){
    char buf[200]; int ret = httpd_req_recv(req,buf,sizeof(buf)-1);
    if(ret<=0) return ESP_FAIL;
    buf[ret]='\0';
    char ssid[32]={0}, pass[64]={0}, token[64]={0};
    sscanf(buf,"ssid=%31[^&]&pass=%63[^&]&token=%63s",ssid,pass,token);
    save_wifi_config(ssid,pass);
    save_device_token(token);

    const char *resp="<html><body><h2>Saved! Rebooting...</h2></body></html>";
    httpd_resp_send(req,resp,HTTPD_RESP_USE_STRLEN);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}

static httpd_uri_t root_uri={.uri="/",.method=HTTP_GET,.handler=root_get_handler};
static httpd_uri_t wifi_post_uri={.uri="/wifi",.method=HTTP_POST,.handler=wifi_post_handler};

static void start_webserver(void){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server,&config));
    httpd_register_uri_handler(server,&root_uri);
    httpd_register_uri_handler(server,&wifi_post_uri);
}

/* ===================== AP MODE ===================== */
static void wifi_init_ap(void){
;
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config={
        .ap={
            .ssid="ESP32_SETUP",
            .ssid_len=11,
            .password="12345678",
            .max_connection=4,
            .authmode=WIFI_AUTH_WPA_WPA2_PSK
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG,"SoftAP started, SSID:ESP32_SETUP, Password:12345678");
    start_webserver();
}

/* ===================== STA MODE ===================== */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id,void* event_data){
    if(event_base==WIFI_EVENT){
        switch(event_id){
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                ESP_LOGI(TAG,"STA_START connecting...");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG,"Disconnected from Wi-Fi");
                if(retry_count<MAX_RETRY){
                    esp_wifi_connect();
                    retry_count++;
                } else {
                    ESP_LOGW(TAG,"Max retries reached, fallback to AP");
                    wifi_init_ap();
                }
                break;
            default: break;
        }
    } else if(event_base==IP_EVENT && event_id==IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG,"Connected! IP:" IPSTR,IP2STR(&event->ip_info.ip));
        wifi_manager_on_connected();
    }
}

static void wifi_init_sta(const char *ssid,const char *pass){
    esp_log_level_set("wifi", ESP_LOG_DEBUG); // Bật log debug cho Wi-Fi
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,
                        &wifi_event_handler,NULL,NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,
                        &wifi_event_handler,NULL,NULL));

    wifi_config_t wifi_config={0};
    strcpy((char*)wifi_config.sta.ssid,ssid);
    strcpy((char*)wifi_config.sta.password,pass);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* ===================== MAIN ENTRY ===================== */
void wifi_manager_start(void){
    char ssid[32], pass[64];
    if(load_wifi_config(ssid,pass)){
        ESP_LOGI(TAG,"Connecting using stored Wi-Fi...");
        wifi_init_sta(ssid,pass);
    } else {
        ESP_LOGW(TAG,"No Wi-Fi config found, starting AP mode...");
        wifi_init_ap();
    }
}
