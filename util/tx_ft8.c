#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "tx_ft8.h"

#include "pico/stdlib.h"
#include "GUI.h"

#include "../peripheral_util/pico_si5351/si5351.h"
#include "../peripheral_util/pico_keypad4x4/pico_keypad4x4.h"

void tune(int RF_freq){

    //just transmit a single tone for testing
}

void send_ft8(uint8_t tones[], uint32_t RF_freq, uint16_t AF_freq){

    absolute_time_t tone_wait_time;

    uint32_t tx_freq;
    for (uint8_t i = 0; i < 79; i++){
        //uint32_t tone_start_time = time_us_32();
        //tone_wait_time._private_us_since_boot = tone_start_time + 160000;
        tone_wait_time._private_us_since_boot = time_us_32() + 160000;
        tx_freq = 4 * (RF_freq + AF_freq + tones[i] * 6.25);
        printf("%d ", tones[i]);
        
        SI_SETFREQ(1, tx_freq);
    	si_evaluate();

        update_progress_bar(false, i);

        update_tx_waterfall(tones, AF_freq, i);
    	
        char abort_key = pico_keypad_get_key();
        if (abort_key == '*'){
            printf("aborted sending\n");
            break;
        }

        //get swr
        //if not enough time in 160ms, I can run this in second thread
        //sleep_ms(160);
        busy_wait_until(tone_wait_time);
    }

    printf("\ndone sending\n");
}

float get_swr(){
    //take two voltage measurements from an swr bridge, then calculate swr
}

