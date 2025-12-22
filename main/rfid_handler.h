#ifndef RFID_HANDLER_H
#define RFID_HANDLER_H

#include "rc522.h"

// Global variables
extern rc522_driver_handle_t s_rc522_driver;
extern rc522_handle_t s_rc522;
extern int add_card;

// Initialize RFID reader
void init_rfid(void);

// Save UID to NVS
esp_err_t save_uid(const char *uid);

// Load UID from NVS
char* load_uid(void);

#endif // RFID_HANDLER_H
