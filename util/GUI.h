#include <stdbool.h>
#include <stdint.h>

#include "../ft8/decode_ft8.h"

//#include "pico/util/datetime.h"

typedef struct {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
}hms;

void initialize_screen();

void set_config_menu(hms *current, hms *config , uint64_t *config_us);

void set_default_GUI();

//void update_time_display(datetime_t *gtime);

void update_current_time(hms *current, hms *config , uint64_t config_us);

void update_time_display(hms *current);

void update_frequency_display(int RF_freq);

void update_waterfall(int start_block);

void update_tx_waterfall(uint8_t tones[], uint16_t af_freq, uint8_t tone_idx);

void update_progress_bar(bool rx, int progress_block);

void reset_progress_bar();

void clear_main_messages();

void clear_my_messages();

void update_main_messages(message_info mes_list[], uint8_t num_decoded, bool cq_only, hms *current);

void add_to_my_messages(char display_message[], uint16_t message_bg_color);

void update_current_station_display(message_info cur, bool same);
