#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "gfx.h"
#include "glcdfont.c"

#include "st7789.h"

//Written 10/9/21 by Godwin Duan AA1GD
//Based on Adafruit-GFX-Library

//maybe define color values here?

const uint screen_width = 240;
const uint screen_height = 320;

void drawFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
    for (int16_t i = x; i < x + w; i++) {
        drawFastVLine(i, y, h, color);
    }
}
void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color){
    for (int16_t i = y; i < y + h; i++){
        st7789_set_cursor(x, i);
        st7789_put(color);
    }
}

void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color){
    for (int16_t i = x; i < x + w; i++){
        st7789_set_cursor(i, y);
        st7789_put(color);
    }
}

void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size){

    if ((x >= screen_width) ||              // Clip right
        (y >= screen_height) ||             // Clip bottom
        ((x + 6 * size - 1) < 0) || // Clip left
        ((y + 8 * size - 1) < 0))   // Clip top
      return;

    //if (!_cp437 && (c >= 176)) c++; // Handle 'classic' charset behavior 
    //No idea what this does

    for (int8_t i = 0; i < 5; i++) { // Char bitmap = 5 columns
      uint8_t line = font[c * 5 + i];
      for (int8_t j = 0; j < 8; j++, line >>= 1) {
        if (line & 1) {
          if (size == 1){
            //writePixel(x + i, y + j, color);
            st7789_set_cursor(x + i, y + j);
            st7789_put(color);
          } else
            drawFillRect(x + i * size, y + j * size, size, size, color);
        } else if (bg != color) {
          if (size == 1){
            //writePixel(x + i, y + j, bg);
            st7789_set_cursor(x + i, y + j);
            st7789_put(bg);
          } else
            drawFillRect(x + i * size, y + j * size, size, size, bg);
        }
      }
    }
    if (bg != color) { // If opaque, draw vertical line for last column
      if (size == 1)
        drawFastVLine(x + 5, y, 8, bg);
      else
        drawFillRect(x + 5 * size, y, size, 8 * size, bg);
    }
}

void drawString(int16_t x, int16_t y, char *text, uint16_t color, uint16_t bg, uint8_t size){
    
    uint8_t text_length = strlen(text);

    for (uint8_t i = 0; i < text_length; i++){
        //prevents running off screen. Maybe I should make it loop around?
        if ((x + size * i * 6) >= screen_width){
            return;
        }
        
        char temp_char = text[i];

        drawChar(x + size * i * 6, y, temp_char, color, bg, size);
    }

    //maybe should return where the cursor is now
    //seems a bit slow. May want to try st7789_write.
}
