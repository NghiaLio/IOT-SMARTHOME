#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <stdbool.h>

// Biến global để kiểm tra trạng thái WiFi
extern bool wifi_connected;

// Hàm khởi tạo WiFi Station mode
void wifi_init_sta(void);

#endif // WIFI_HANDLER_H
