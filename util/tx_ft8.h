#include <stdint.h>

void tune(int RF_freq);

void send_ft8(uint8_t tones[], uint32_t RF_freq, uint16_t AF_freq);

float get_swr();