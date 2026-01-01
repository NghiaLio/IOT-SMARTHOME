#include "rfid_handler.h"
#include "config.h"
#include "actuators.h"
#include "firebase_handler.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "RFID";

rc522_driver_handle_t s_rc522_driver = NULL;
rc522_handle_t s_rc522 = NULL;
int add_card = 0;

esp_err_t save_uid(const char *uid) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_str(handle, "stored_uid", uid);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

char* load_uid(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) return NULL;
    size_t len;
    err = nvs_get_str(handle, "stored_uid", NULL, &len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return NULL;
    }
    char *uid = malloc(len);
    if (!uid) {
        nvs_close(handle);
        return NULL;
    }
    err = nvs_get_str(handle, "stored_uid", uid, &len);
    nvs_close(handle);
    if (err != ESP_OK) {
        free(uid);
        return NULL;
    }
    return uid;
}

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        char uid_str[RC522_PICC_UID_STR_BUFFER_SIZE_MAX] = {0};
        if (rc522_picc_uid_to_str(&picc->uid, uid_str, sizeof(uid_str)) == ESP_OK) {
            ESP_LOGI(TAG, "RFID: UID=%s, Type=%s", uid_str, rc522_picc_type_name(picc->type));
            
            if (add_card == 1) {
                if (save_uid(uid_str) == ESP_OK) {
                    ESP_LOGI(TAG, "The da duoc luu: %s", uid_str);
                    add_card = 0;
                    send_to_firebase("{\"addCard\":0}", HTTP_METHOD_PATCH, "data.json");
                    char json_card[64];
                    sprintf(json_card, "{\"card\":\"%s\"}", uid_str);
                    send_to_firebase(json_card, HTTP_METHOD_POST, "card.json");
                } else {
                    ESP_LOGE(TAG, "Loi luu UID");
                }
            } else {
                char *stored_uid = load_uid();
                if (stored_uid && strcmp(uid_str, stored_uid) == 0) {
                    ESP_LOGI(TAG, "Mo cua cho UID: %s", uid_str);
                    set_servo_angle(90);
                    door_angle = 90;
                    buzzer_beep_short();
                    esp_timer_start_once(door_timer, 5000000);
                } else {
                    ESP_LOGI(TAG, "UID khong hop le: %s", uid_str);
                }
                if (stored_uid) free(stored_uid);
            }
        } else {
            ESP_LOGI(TAG, "RFID: Card detected (failed to format UID)");
        }
        blink_led();
    } else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "RFID: Card removed");
    }
}

void init_rfid(void) {
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
        .rst_io_num = RC522_RST_GPIO,
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
