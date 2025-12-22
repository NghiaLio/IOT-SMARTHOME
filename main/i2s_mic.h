#ifndef I2S_MIC_H
#define I2S_MIC_H

#include "driver/i2s_std.h"

// I2S channel handle
extern i2s_chan_handle_t rx_handle;

// Initialize I2S microphone
void init_microphone(void);

#endif // I2S_MIC_H
