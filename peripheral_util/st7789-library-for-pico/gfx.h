#pragma once
#include <stdlib.h>
#include <stdint.h>

void drawFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);

void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);

void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size);

void drawString(int16_t x, int16_t y, char *text, uint16_t color, uint16_t bg, uint8_t size);

//add drawString no BG