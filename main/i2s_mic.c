#include "i2s_mic.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "I2S_MIC";

i2s_chan_handle_t rx_handle = NULL;

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
            .mclk = I2S_GPIO_UNUSED,
            .dout = I2S_GPIO_UNUSED 
        },
    };
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    
    i2s_channel_init_std_mode(rx_handle, &std_cfg); 
    i2s_channel_enable(rx_handle);
    
    ESP_LOGI(TAG, "I2S Microphone initialized");
}
