#include "actuators.h"
#include "config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "ACTUATORS";

// Device states
int led_state = 0;
int fan_state = 0;
int door_angle = 0;
int ac_state = 0;
int rain_angle = 0;

// Timer handles
esp_timer_handle_t led_blink_timer;
esp_timer_handle_t door_timer;

// Buzzer task
TaskHandle_t buzzer_task_handle = NULL;
bool buzzer_active = false;

// Callback to turn off LED after blink
void led_blink_callback(void *arg) {
    gpio_set_level(LED_PIN, 0);
}

// Callback to close door after delay
void door_close_callback(void *arg) {
    set_servo_angle(0);
    door_angle = 0;
    ESP_LOGI(TAG, "Door closed");
}

// Buzzer beep task
static void buzzer_beep_task(void *arg) {
    while (buzzer_active) {
        gpio_set_level(BUZZER_PIN, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_level(BUZZER_PIN, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

// Function to start buzzer alarm
void start_buzzer_alarm(void) {
    if (!buzzer_active) {
        buzzer_active = true;
        xTaskCreate(buzzer_beep_task, "buzzer_alarm", 2048, NULL, 5, &buzzer_task_handle);
    }
}

// Function to stop buzzer alarm
void stop_buzzer_alarm(void) {
    if (buzzer_active) {
        buzzer_active = false;
        if (buzzer_task_handle != NULL) {
            vTaskDelete(buzzer_task_handle);
            buzzer_task_handle = NULL;
        }
        gpio_set_level(BUZZER_PIN, 0);
    }
}

// Function to blink LED for 1 second
void blink_led(void) {
    gpio_set_level(LED_PIN, 1);
    esp_timer_start_once(led_blink_timer, 1000000);
}

// Function for short buzzer beep
void buzzer_beep_short(void) {
    gpio_set_level(BUZZER_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(BUZZER_PIN, 0);
}

void init_led(void) {
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(RELAY_FAN_PIN);
    gpio_set_direction(RELAY_FAN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_FAN_PIN, 1);
    gpio_reset_pin(RELAY_AC_PIN);
    gpio_set_direction(RELAY_AC_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_AC_PIN, 1);
    
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
    
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER_PIN, 0);
    
    // Init LED blink timer
    esp_timer_create_args_t timer_args = {
        .callback = led_blink_callback,
        .name = "led_blink"
    };
    esp_timer_create(&timer_args, &led_blink_timer);
    
    // Init door close timer
    esp_timer_create_args_t door_timer_args = {
        .callback = door_close_callback,
        .name = "door_close"
    };
    esp_timer_create(&door_timer_args, &door_timer);
}

void init_servo(void) {
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

    // Config for rain servo
    ledc_timer_config_t rain_timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = RAIN_SERVO_TIMER,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&rain_timer_conf);

    ledc_channel_config_t rain_channel_conf = {
        .gpio_num = RAIN_SERVO_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = RAIN_SERVO_CHANNEL,
        .timer_sel = RAIN_SERVO_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&rain_channel_conf);
}

void set_servo_angle(int angle) {
    uint32_t duty = (SERVO_MIN_PULSEWIDTH + (SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * angle / SERVO_MAX_DEGREE) * (1 << LEDC_TIMER_13_BIT) / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_LIGHT_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_LIGHT_CHANNEL);
}

void set_rain_servo_angle(int angle) {
    uint32_t duty = (SERVO_MIN_PULSEWIDTH + (SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * angle / SERVO_MAX_DEGREE) * (1 << LEDC_TIMER_13_BIT) / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, RAIN_SERVO_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, RAIN_SERVO_CHANNEL);
}
