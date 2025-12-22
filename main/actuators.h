#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <stdbool.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Device states
extern int led_state;
extern int fan_state;
extern int door_angle;
extern int ac_state;
extern int rain_angle;

// Timer handles
extern esp_timer_handle_t led_blink_timer;
extern esp_timer_handle_t door_timer;

// Buzzer task
extern TaskHandle_t buzzer_task_handle;
extern bool buzzer_active;

// Initialization functions
void init_led(void);
void init_servo(void);

// Control functions
void blink_led(void);
void set_servo_angle(int angle);
void set_rain_servo_angle(int angle);
void buzzer_beep_short(void);
void start_buzzer_alarm(void);
void stop_buzzer_alarm(void);

// Timer callbacks
void led_blink_callback(void *arg);
void door_close_callback(void *arg);

#endif // ACTUATORS_H
