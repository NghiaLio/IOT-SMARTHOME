#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"

// ============================================================
// CẤU HÌNH NGƯỜI DÙNG
// ============================================================
#define WIFI_SSID           "TP-Link_FA7F"
#define WIFI_PASS           "24681537"
#define WIT_ACCESS_TOKEN    "SNQYPCD3R67FQST7SMPDVYCE4GJNNV5V"

// Firebase config
#define FIREBASE_PROJECT_ID "smart-944cb"
#define FIREBASE_DEVICE_ID  "esp123"
#define FIREBASE_API_KEY   "your-api-key"

// ============================================================
// CẤU HÌNH CHÂN GPIO
// ============================================================
// I2S Microphone pins
#define I2S_SCK_PIN         GPIO_NUM_11
#define I2S_WS_PIN          GPIO_NUM_12
#define I2S_SD_PIN          GPIO_NUM_10
// #define I2S_MCLK_PIN        GPIO_NUM_13
#define LED_PIN             GPIO_NUM_2

// RFID RC522 pins
#define RC522_SDA_GPIO      5
#define RC522_SCK_GPIO      21   
#define RC522_MOSI_GPIO     20
#define RC522_MISO_GPIO     19
#define RC522_RST_GPIO      -1

// Relay pins
#define RELAY_FAN_PIN       GPIO_NUM_14
#define RELAY_AC_PIN        GPIO_NUM_13

// Servo pins
#define SERVO_LIGHT_PIN     GPIO_NUM_8
#define SERVO_LIGHT_CHANNEL LEDC_CHANNEL_0
#define SERVO_LIGHT_TIMER   LEDC_TIMER_0
#define RAIN_SERVO_PIN      GPIO_NUM_9
#define RAIN_SERVO_CHANNEL  LEDC_CHANNEL_1
#define RAIN_SERVO_TIMER    LEDC_TIMER_1
#define SERVO_MIN_PULSEWIDTH 500
#define SERVO_MAX_PULSEWIDTH 2500
#define SERVO_MAX_DEGREE     180

// DHT11 pin
#define DHT_PIN             GPIO_NUM_15

// MQ2 Gas Sensor
#define GAS_PIN             GPIO_NUM_7
#define GAS_ADC_CHANNEL     ADC_CHANNEL_6

// Flame Sensor
#define FLAME_PIN           GPIO_NUM_16

// Rain Sensor
#define RAIN_PIN            GPIO_NUM_17

// Buzzer
#define BUZZER_PIN          GPIO_NUM_4

// ============================================================
// CẤU HÌNH GHI ÂM
// ============================================================
#define SAMPLE_RATE         16000
#define REC_TIME_SEC        1.5
#define RECORD_SIZE         (int)(SAMPLE_RATE * REC_TIME_SEC * 2)
#define SOUND_THRESHOLD     3000

// ============================================================
// CẤU HÌNH KHÁC
// ============================================================
#define FIREBASE_BUFFER_SIZE 512

#endif // CONFIG_H
