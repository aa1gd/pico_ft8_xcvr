#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include "GUI.h"
#include "../ft8/decode_ft8.h"

#include "../peripheral_util/st7789-library-for-pico/st7789.h"
#include "../peripheral_util/st7789-library-for-pico/gfx.h"
#include "hardware/spi.h"
#include "../peripheral_util/pico_keypad4x4/pico_keypad4x4.h"
//#include "hardware/rtc.h"
//#include "pico/util/datetime.h"

//may need to include hardware/timer.h?

#define LCD_WIDTH 240
#define LCD_HEIGHT 320

// lcd configuration
const struct st7789_config lcd_config = {
    .spi = PICO_DEFAULT_SPI_INSTANCE,
    .gpio_din = PICO_DEFAULT_SPI_TX_PIN,
    .gpio_clk = PICO_DEFAULT_SPI_SCK_PIN,
    .gpio_cs = PICO_DEFAULT_SPI_CSN_PIN,
    .gpio_dc = 16,
    .gpio_rst = 20,
    .gpio_bl = 21,
};

// if waterfall_height can be changed in menu, these should not be const
const uint16_t label_bar_height = 16;
const uint16_t waterfall_height = 16; //this should be increased/decreased in multiples of 8. Increase here
//takes away from my message space
const uint16_t lb1y = label_bar_height + waterfall_height;
const uint16_t lb2y = lb1y + label_bar_height + kMax_decoded_messages * 8;
const uint16_t menu_height = 80; // includes one label bar height

const uint16_t messages_for_me_height = LCD_HEIGHT - menu_height - label_bar_height * 3 - waterfall_height - kMax_decoded_messages * 8;
const uint16_t max_num_messages_for_me = messages_for_me_height / 8;

const uint8_t bins_per_pixel = num_bins / LCD_WIDTH;

void initialize_screen()
{
    st7789_init(&lcd_config, LCD_WIDTH, LCD_HEIGHT);
    st7789_fill(0x0008);
    drawString(50, 180, "Loading... :D", 0xffff, 0x0008, 2);
    drawString(5, 60, "Pico FT8 XCVR", 0xffff, 0x0008, 3);
    drawString(5, 100, "v1.0 by AA1GD", 0xffff, 0x0009, 3);
}

void set_config_menu(hms *current, hms *config , uint64_t *config_us)
{
    st7789_fill(0x0000);
    drawString(0, 0, "How to Setup:", 0xffff, 0x0000, 2);
    drawString(0, 16, "Get a clock with accurate UTC time ready.", 0xffff, 0x0000, 1);
    drawString(0, 24, "When you're ready, press the * key.", 0xffff, 0x0000, 1);
    drawString(0, 32, "At the same time, note the UTC time,", 0xffff, 0x0000, 1);
    drawString(0, 40, "then input it using the keypad.", 0xffff, 0x0000, 1);
    drawString(0, 48, "You can restart by pressing #.", 0xffff, 0x0000, 1);
    drawString(0, 56, "Press D when you're done.", 0xffff, 0x0000, 1);

    uint8_t time_index = 0;
    drawString(0, 72, "You are adjusting digit:", 0xFFE0, 0x0000, 1);
    drawChar(150, 72, time_index + 49, 0xFFE0, 0x0000, 1); //index displayed starting at one (+48 would be zero)
    while(true){
        char config_key = pico_keypad_get_key();
        if (time_index > 5){
                drawFillRect(114, 100, 12,16, 0x0000);
                drawChar(114, 100, '!', 0xFFFF, 0xF800, 2);
            } else
        drawChar(150, 72, time_index + 49, 0xFFE0, 0x0000, 1);

        if (config_key == '*'){
            *config_us = time_us_64();
            drawChar(114, 100, '*', 0xFFFF, 0xF800, 2);
        } else if (config_key == '#'){
            time_index = 0;
            *config_us = 0;
            config->hour = 0;
            config->min = 0;
            config->sec = 0;
            drawFillRect(114, 100, 12,16, 0x0000);
        } else if (config_key == 'D'){
            return;
        } else if (config_key > 47 && config_key < 58){ //numbers in ascii
            int config_int = char_to_int(config_key);
            if (time_index == 0){
                config->hour = 10 * config_int;
                time_index++;
            } else if (time_index == 1){
                config->hour += config_int;
                time_index++;
            } else if (time_index == 2){
                config->min = 10 * config_int;
                time_index++;
            } else if (time_index == 3){
                config->min += config_int;
                time_index++;
            } else if (time_index == 4){
                config->sec = 10* config_int;
                time_index++;
            } else if (time_index == 5){
                config->sec += config_int;
                time_index++;
            } 
        }
        sleep_ms(200);
        update_current_time(current, config, *config_us);
        update_time_display(current);
    }


}

void set_default_GUI()
{
    st7789_fill(0xffff);
    drawFillRect(0, 0, LCD_WIDTH, 10, 0xffff); //fft labels
    for (int i = 0; i < LCD_WIDTH; i += 20)    //draw bars
    {
        uint8_t bar_height = !(i % 40) ? 6 : 3;
        drawFastVLine(i, label_bar_height - bar_height, bar_height, 0x0000);
    }
    //I'll divide the screen vertically into 40 bars of 8
    drawString(68, 2, "1000", 0x0000, 0xffff, 1);
    drawString(148, 2, "2000", 0x0000, 0xffff, 1);
    drawFillRect(0, label_bar_height, LCD_WIDTH, waterfall_height, 0x0008);
    drawFillRect(0, lb1y, LCD_WIDTH, 16, 0xD69A); //grey
    drawString(12, lb1y + 4, "UTC", 0x0000, 0xD69A, 1);
    drawString(42, lb1y + 4, "dB", 0x0000, 0xD69A, 1);
    drawString(66, lb1y + 4, "DT", 0x0000, 0xD69A, 1);
    drawString(96, lb1y + 4, "Freq", 0x0000, 0xD69A, 1);
    drawString(126, lb1y + 4, "Message", 0x0000, 0xD69A, 1);
    drawFillRect(0, lb2y, LCD_WIDTH, 16, 0xD69A); //label "TO ME"
    drawString(12, lb2y + 4, "UTC", 0x0000, 0xD69A, 1);
    drawString(42, lb2y + 4, "dB", 0x0000, 0xD69A, 1);
    drawString(66, lb2y + 4, "DT", 0x0000, 0xD69A, 1);
    drawString(96, lb2y + 4, "Freq", 0x0000, 0xD69A, 1);
    drawString(126, lb2y + 4, "Messages FOR ME", 0x0000, 0xD69A, 1);
    drawFillRect(0, LCD_HEIGHT - menu_height, LCD_WIDTH, label_bar_height, 0xE71C); //tune, set freq, set time, abort, toggle autosend
    drawString(3, LCD_HEIGHT - menu_height + 4, "Time #A", 0x0000, 0xCE59, 1); //slightly darker gray
    drawString(51, LCD_HEIGHT - menu_height + 4, "Freq #B", 0x0000, 0xCE59, 1);
    drawString(99, LCD_HEIGHT - menu_height + 4, "Auto #C", 0x0000, 0xCE59, 1); //the bg can change to green when enabled
    drawString(147, LCD_HEIGHT - menu_height + 4, "Tune #D", 0x0000, 0xCE59, 1);
    drawString(195, LCD_HEIGHT - menu_height + 4, "Abort *", 0x0000, 0xF800, 1); //red
    
    //drawFillRect(0, LCD_HEIGHT - menu_height - label_bar_height, 100, menu_height - label_bar_height, 0x0000);               //black box w/ yellow for time, freq
    drawFillRect(102, LCD_HEIGHT - 8, num_blocks, 8, 0xD69A); //progress bar

    drawString(3, LCD_HEIGHT - 48 + 4, "RX CALL", 0x0000, 0xAEDC, 1); //bg is pale blue
    drawString(3 + 6*8, LCD_HEIGHT - 48 + 4, "RX GRID", 0x0000, 0xAEDC, 1);
    drawString(100, LCD_HEIGHT - 64, "Freq:", 0x0000, 0xAEDC, 1);

    drawString(188, LCD_HEIGHT - menu_height + 16, "Gen Msgs", 0x0000, 0xAEDC, 1);
    drawString(188, LCD_HEIGHT - menu_height + 24, "(0) CQ", 0x0000, 0xF79E, 1); //gets darker down the list
    drawString(188, LCD_HEIGHT - menu_height + 32, "(1) Grid", 0x0000, 0xEF5D, 1);
    drawString(188, LCD_HEIGHT - menu_height + 40, "(2) SNR", 0x0000, 0xE73C, 1);
    drawString(188, LCD_HEIGHT - menu_height + 48, "(3) RSNR", 0x0000, 0xE71C, 1);
    drawString(188, LCD_HEIGHT - menu_height + 56, "(4) RRR", 0x0000, 0xDEFB, 1);
    drawString(188, LCD_HEIGHT - menu_height + 64, "(5) RR73", 0x0000, 0xDEDB, 1);
    drawString(188, LCD_HEIGHT - menu_height + 72, "(6) 73", 0x0000, 0xCE59, 1);
}

void update_frequency_display(int RF_freq)
{
    char rf_freq_string[8];
    sprintf(rf_freq_string, "%08d", RF_freq);
    //drawString(0, LCD_HEIGHT - menu_height + label_bar_height, rf_freq_string, 0xFFE0, 0x0000, 2); //yellow
    drawString(0, 256, rf_freq_string, 0xFFE0, 0x0000, 2); //yellow
}

//yes, I know there's a function that does this, but it gives month/year/dow too
//this seems really slow...
//replaced with time functions
/*
void update_time_display(datetime_t *gtime)
{
    rtc_get_datetime(gtime);
    uint8_t hour = gtime->hour;
    uint8_t min = gtime->min;
    uint8_t sec = gtime->sec;
    char time_in_string[8];
    sprintf(time_in_string, "%02d:%02d:%02d", hour, min, sec);
    drawString(0, LCD_HEIGHT - 16, time_in_string, 0xFFE0, 0x0000, 2); //yellow
}
*/

void update_current_time(hms *current, hms *config , uint64_t config_us){
    uint32_t tsecs = (time_us_64() - config_us) / 1000000;
    uint32_t tmins = tsecs / 60;
    uint32_t thours = tmins / 60;

    current->sec = tsecs % 60;
    current->min = tmins % 60;
    current->hour = thours % 24;

    current->sec += config->sec;
    if (current->sec >= 60){
        current->sec = current->sec % 60;
        current->min++;
    }

    current->min += config->min;
    if (current->min >= 60){
        current->min = current->min % 60;
        current->hour++;
    }

    current->hour += config->hour;
    if (current->hour >= 24){
        current->hour = current->hour % 60;
    }
}

void update_time_display(hms *current){
    char time_in_string[8];
    sprintf(time_in_string, "%02d:%02d:%02d", current->hour, current->min, current->sec);
    drawString(0, LCD_HEIGHT - 16, time_in_string, 0xFFE0, 0x0000, 2); //yellow
}

void update_waterfall(int start_block)
{
    //adding (int y = waterfall_height - 1; y >= 0; y--) can make the waterfall go backwards
    for (int y = 0; y < waterfall_height; y++)
    {
        for (int x = 0; x < num_bins / bins_per_pixel; x++)
        {
            //stronger signals make brighter colors
            uint16_t watercolor = 0;
            if (start_block - y >= 0)
            { //prevents from reaching outside of the array
                watercolor = power.mag[(start_block - y) * power.time_osr * power.freq_osr * power.num_bins + x * bins_per_pixel];
            }

            if (watercolor < 128)
            {
                watercolor = 0x0008; //kind of like squelch
            }
            else if (watercolor == 255)
            {
                watercolor = 0xF801; //marking max magnitudes with red
            }
            else
            {
                watercolor *= 16;
            }

            st7789_set_cursor(x, y + label_bar_height);
            st7789_put(watercolor);
            //st7789_set_cursor(x+1,y);
            //st7789_put(watercolor);
        }
    }
}

void update_tx_waterfall(uint8_t tones[], uint16_t af_freq, uint8_t tone_idx){
    uint16_t af_bin = (uint16_t) (af_freq / 6.25f);
    drawFillRect(0, label_bar_height, LCD_WIDTH, waterfall_height, 0x0008);
    for (uint8_t y = 0; y < waterfall_height; y++){
        drawFillRect(af_bin / bins_per_pixel, label_bar_height, 50, waterfall_height, 0x0008);
        if (tone_idx - y < 79){
            uint16_t x = (tones[tone_idx - y] + af_bin) / bins_per_pixel;
            st7789_set_cursor(x, y + label_bar_height);
            st7789_put(0xF800);
        }
    }

}
void update_progress_bar(bool rx, int progress_block)
{
    //uint8_t progress_block_width = (240-100)/num_blocks; //this is equal to one.
    // I might as well draw Vertical lines instead of boxes
    uint16_t progress_color;
    progress_color = rx ? 0x07E0 : 0xF800; //green, red
    drawFastVLine(102 + progress_block, LCD_HEIGHT - 8, 8, progress_color);
}

void reset_progress_bar()
{
    drawFillRect(102, LCD_HEIGHT - 8, num_blocks, 8, 0xD69A);
}

void clear_main_messages()
{
    drawFillRect(0, lb1y + label_bar_height, LCD_WIDTH, 8 * kMax_decoded_messages, 0xffff);
}

void clear_my_messages()
{
    drawFillRect(0, lb2y + label_bar_height, LCD_WIDTH, messages_for_me_height, 0xffff);
}

void update_main_messages(message_info mes_list[], uint8_t num_decoded, bool cq_only, hms *current)
{
    static uint8_t main_message_y_index = 0;
    //update_current_time before this is called

    if (main_message_y_index + num_decoded > kMax_decoded_messages)
    {
        clear_main_messages();
        main_message_y_index = 0;
    }

    for (uint8_t i = 0; i < num_decoded; i++)
    {   
        uint16_t message_bg_color = 0xFFFF;
        bool for_me = false;

        if (mes_list[i].addressed_to_me){
            message_bg_color = 0xF965; //tart red
            for_me = true;
        }

        //if in a frequency range
        char display_message[40];
        //up to the string is 21 spaces
        //leaving 19 chars for message
        sprintf(display_message, "%02d%02d%02d %3d %4.1f %4d %-19s", current->hour, current->min, current->sec
        ,mes_list[i].self_rx_snr, mes_list[i].time_offset, mes_list[i].af_frequency, mes_list[i].full_text);

        if (mes_list[i].type_cq){
            message_bg_color = 0x3646; //lime green
        }

        if (for_me){
            add_to_my_messages(display_message, message_bg_color);
        }
        
        if (cq_only){
            continue;
        }

        drawString(0,lb1y + label_bar_height + 8 * main_message_y_index, display_message, 0x0000, message_bg_color, 1);
        main_message_y_index++;
    }
}

void add_to_my_messages(char display_message[], uint16_t message_bg_color)
{
    static uint8_t my_message_y_index = 0;
    if (my_message_y_index >= max_num_messages_for_me)
    {
        my_message_y_index = 0;
        clear_my_messages();
    }

    drawString(0,lb2y + label_bar_height + 8 * my_message_y_index, display_message, 0x0000, message_bg_color, 1);
    my_message_y_index++;
}

void update_current_station_display(message_info cur, bool same){

    if (!same){
        drawFillRect(3, LCD_HEIGHT - 48 + 12, 100, 8, 0xffff); // reset rx call and rx grid
        drawFillRect(148, LCD_HEIGHT - 64, 24, 8, 0xffff); // reset freq
    }

    drawString(3, LCD_HEIGHT - 48 + 12, cur.station_callsign, 0x0000, 0xffff, 1);
    drawString(3 + 6*8, LCD_HEIGHT - 48 + 12, cur.grid_square, 0x0000, 0xffff, 1);
    char freq_in_string[4];
    sprintf(freq_in_string, "%4d", cur.af_frequency);
    drawString(148, LCD_HEIGHT - 64, freq_in_string, 0x0000, 0xffff, 1);
}