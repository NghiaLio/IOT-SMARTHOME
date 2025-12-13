idf_component_register(SRCS "i2s.c"
                       INCLUDE_DIRS "."
                        REQUIRES esp_wifi esp_http_client esp_event nvs_flash esp_system lwip esp_netif esp_http_server driver esp-tls esp_adc esp_timer esp-sr hardware_driver player)
