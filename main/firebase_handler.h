#ifndef FIREBASE_HANDLER_H
#define FIREBASE_HANDLER_H

#include "esp_http_client.h"

// Send data to Firebase
char* send_to_firebase(char *json_data, esp_http_client_method_t method, char *path);

// Get data from Firebase
void get_firebase_data(void);

#endif // FIREBASE_HANDLER_H
