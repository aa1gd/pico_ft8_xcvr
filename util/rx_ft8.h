#pragma once

#include <stdint.h>
#include "../ft8/decode_ft8.h"

#define CAPTURE_CHANNEL 0
#define DC_BIAS 2048

extern int16_t fresh_signal[block_size];

void setup_adc();

void collect_adc();

void set_rx_freq(uint32_t);

void set_sideband(); //90 for usb, 270 for lsb