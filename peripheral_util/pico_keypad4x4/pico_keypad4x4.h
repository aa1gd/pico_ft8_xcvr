//Github link: https://github.com/OSWA00/pico-keypad4x4
#pragma once

#include "pico/stdlib.h"
#include "hardware/timer.h"

void pico_keypad_init(uint columns[4], uint rows[4], char matrix_values[16]);

char pico_keypad_get_key(void);

void pico_keypad_irq_enable(bool enable, gpio_irq_callback_t callback);

int char_to_int(char c);

