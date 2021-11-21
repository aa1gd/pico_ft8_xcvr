#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "ft8/pack.h"
#include "ft8/encode.h"

#include "ft8/decode_ft8.h"
#include "ft8/gen_ft8.h"

#include "util/tx_ft8.h"
#include "util/rx_ft8.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"

#include "hardware/adc.h"
#include "hardware/dma.h"

#include "pico/multicore.h"
#include "hardware/irq.h"

//uncomment if overclocking >290MHz
//#include "hardware/vreg.h"

// RTC was too slow. So I am using hardware_timer's time_us_64

#include "peripheral_util/pico_keypad4x4/pico_keypad4x4.h"
#include "peripheral_util/st7789-library-for-pico/st7789.h"
#include "peripheral_util/st7789-library-for-pico/gfx.h"
#include "peripheral_util/pico_si5351/si5351.h"

#include "util/GUI.h"

//Created by AA1GD Aug. 25, 2021
//OCTOBER 18, 2021 AT 5:14 PM CDT FIRST ON AIR DECODES WITH THIS
#define MY_CALLSIGN "AA1GD"
#define MY_GRID "EM13"

message_info CurrentStation;

UserSendSelection sendChoices;

message_info message_list[kMax_decoded_messages]; // probably needs to be memset cleared before each decode

// pin name configuration
const uint LED_PIN = 25;
const uint TX_PIN = 0;

uint column_pins[4] = {6, 7, 8, 9};
uint row_pins[4] = {10, 11, 12, 13};
char matrix_characters[16] = {'D', '#', '0', '*', 'C', '9', '8', '7', 'B', '6', '5', '4', 'A', '3', '2', '1'};

uint64_t config_us;

hms config_hms;
hms current_hms;

uint64_t fine_offset_us = 0; //in us

int16_t signal_for_processing[num_samples_processed] = {0};

uint32_t handler_max_time = 0;

void core1_irq_handler()
{
    // Copy capture_buf to signal_for_processing array, take fft and save to power, check for user keypad input
    while (multicore_fifo_rvalid())
    {   
        uint32_t handler_start = time_us_32();
        uint16_t idx = multicore_fifo_pop_blocking();
        //printf("Got this from the fifo: %d\n", idx);
        //printf("a fresh sample: %d\n", fresh_signal[420]);

        /*
        // shift old samples 960 to the left
        for (int i = 0; i < num_samples_processed - block_size; i++)
        { //1920 is nfft
            signal_for_processing[i] = signal_for_processing[i + block_size];
        }

        // copy fresh samples in
        //IF IDX IS LESS THAN 80. else just zeroes
        for (int j = num_samples_processed - block_size; j < num_samples_processed; j++)
        {
            signal_for_processing[j] = fresh_signal[j] - DC_BIAS;
        }

        printf("A random sample: %d\n", signal_for_processing[420]);
        //wait at least 3 sampling times to fill signal_for_processing until extraction
        if (idx < 3)
        {
            break;
        }
        */

        //inc_extract_power(signal_for_processing);
        for(int i = 0; i < nfft; i++){
            fresh_signal[i] -= DC_BIAS;
        }
        //printf("a fresh sample: %d\n", fresh_signal[420]);
        //uint32_t extract_start = time_us_32();
        inc_extract_power(fresh_signal);
        //printf("Extraction time: %d\n", (time_us_32() - extract_start)/1000);

        //update the waterfall display every
        //uint32_t waterfall_start = time_us_32();
        update_waterfall(idx);
        //printf("Waterfall time: %d\n", (time_us_32() - waterfall_start)/1000);


        //update time - too slow to be done every 0.16 seconds
        //update_current_time(&current_hms, &config_hms, config_us);
        //update_time_display(&current_hms);

        //get keypad input
        //char rx_interrupt_char = pico_keypad_get_key();
        //if (interrupt key), goto (somewhere)
        update_progress_bar(true, idx);

        //move keypad input, time display, progress bar to rx_ft8.c?

        //printf("Handler time: %d\n", (time_us_32() - handler_start)/1000);
        uint32_t handler_time = (time_us_32() - handler_start)/1000;
        if (handler_time > handler_max_time){
            handler_max_time = handler_time;
        }
        //handler MUST BE under 160 ms.
    }

    multicore_fifo_clear_irq(); // Clear interrupt
}

void core1_runner(void)
{
    // Configure Core 1 Interrupt
    printf("second core running!\n");
    multicore_fifo_clear_irq();
    irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_irq_handler);

    irq_set_enabled(SIO_IRQ_PROC1, true);

    // Infinite While Loop to wait for interrupt
    while (1)
    {
        tight_loop_contents();
        //printf("second core says: \"I'm Here!\"\n");
        //sleep_ms(1000);
    }
}

int main()
{   
    //overclocking the processor
    //133MHz default, 250MHz is safe at 1.1V and for flash
    //if using clock > 290MHz, increase voltage and add flash divider
    //see https://raspberrypi.github.io/pico-sdk-doxygen/vreg_8h.html

    //vreg_set_voltage(VREG_VOLTAGE_DEFAULT);
    set_sys_clock_khz(250000,true);

    // start serial connection
    stdio_init_all();
    printf("Serial connected");

    //initialize GPIO pins
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    //initialize keypad
    pico_keypad_init(column_pins, row_pins, matrix_characters);

    // initialize the lcd
    initialize_screen();

    // setup the adc
    setup_adc();

    //Initialize the Si5351 Oct. 10, 2021
    uint32_t rf_freq = 7074000;
    si_init();
    SI_SETFREQ(0, rf_freq);

    //Ri is set to divide the given frequency by 4
    //This is to allow Si5351 to make the .25Hz increments for FT8
    SI_SETFREQ(1, rf_freq * 4);
    si_evaluate();

    //make hanning window for fft work
    make_window();

    sleep_ms(1000);

reconfigure:
    set_config_menu(&current_hms, &config_hms, &config_us);

    bool send = false;
    bool justSent = false; //must recieve right after sending
    bool autosend = true;
    bool cq_only = false;

    //launch second core
    multicore_launch_core1(core1_runner);
    set_default_GUI();
    update_frequency_display(rf_freq);
    update_current_time(&current_hms, &config_hms, config_us);
    update_time_display(&current_hms);

    //MAIN PROGRAM LOOP
    while (true)
    {
        char message[25] = {0};
        uint8_t tones[79] = {0};

        if (send)
        {
            if (autosend)
            {
                printf("autosending\n");
                printf("still here? %s\n", CurrentStation.station_callsign);
                auto_gen_message(message, CurrentStation, MY_CALLSIGN, MY_GRID);
            }
            else
            {
                manual_gen_message(message, CurrentStation, sendChoices, MY_CALLSIGN, MY_GRID);
                //reset send choices
                sendChoices.call_cq = sendChoices.send_73 = sendChoices.send_grid = sendChoices.send_RR73 = sendChoices.send_RRR = sendChoices.send_Rsnr = sendChoices.send_snr = false;
            }

            printf("message to be sent: %s\n", message);
            generate_ft8(message, tones);
            //display message in my messages in yellow
            add_to_my_messages(message, 0xFFE0);

            printf("SENDING FOR 12.64 SECONDS\n\n");
            gpio_put(LED_PIN, 1);
            send_ft8(tones, rf_freq, CurrentStation.af_frequency);
            gpio_put(LED_PIN, 0);
            send = false;
            justSent = true;
        }

        else
        {
            printf("RECIEVING FOR 12.8 SECONDS\n\n");

            inc_collect_power();

            printf("Handler max time: %d\n", handler_max_time);
        
            uint32_t decode_begin = time_us_32();
            uint8_t num_decoded = decode_ft8(message_list);
            printf("decoding took this long: %d us\n", time_us_32() - decode_begin);

            decode_begin = time_us_32(); //i'll just reuse this variable

            //testing example
/*             message_list[0].self_rx_snr = -23;
            message_list[0].time_offset = -0.1;
            message_list[0].af_frequency = 2222;
            strcat(message_list[0].full_text, "AA1GD KI5KGU R+04"); */
            //message_list[0].addressed_to_me = true;

            identify_message_types(message_list, MY_CALLSIGN);
            printf("identification took this long: %d us\n", time_us_32() - decode_begin);
            printf("finished identifying message types\n");
            justSent = false;

            reset_progress_bar(); 
            update_current_time(&current_hms, &config_hms, config_us);
            update_time_display(&current_hms);

            update_main_messages(message_list, num_decoded, cq_only, &current_hms);

            //update_main_messages(message_list,1,false,&current_hms);
        }

        int selected_station = -1; //default is no response
        int rTB = -1;              //rTB stands for responseTypeBuffer

        printf("\nWAITING 2 SECONDS FOR USER INPUT\n");
        printf("Enter station to respond to: ");

        if (autosend){
            for (int i = 0; i < kMax_decoded_messages; i++){
                if (message_list[i].addressed_to_me){
                    selected_station = i; //if autosending, finds the first occurence of callsign and automatically responds
                    printf("autoselected to %d\n", selected_station);
                    send = true;
                    break;
                }
            }
        }
        else
        {
            printf("Enter response type: 0 for CQ, 1 for grid, 2 for snr, 3 for Rsnr, 4 for RRR, 5 for RR73, 6 for 73, 7+ for no choice:\n");
        }

        //while modulo greater than 12.8, OR aborted
        uint64_t overtime = (time_us_64() - config_us + config_hms.sec * 1000000) % 15000000;
        if (overtime < 12800000){
            printf("no time to select\n");
            sleep_us(13000000 - overtime);
        }
        while ((time_us_64() - config_us + config_hms.sec * 1000000) % 15000000 > 12800000)
        {
            //scan for keypad input... otherwise do nothing
            char char_selection = pico_keypad_get_key();

            if (char_selection == '*')
            { //make this the abort/menu key
                //goto back to setup
                goto reconfigure;
            }

            if (justSent)
            {
                continue;
            }

            if (!send || autosend)
            {
                selected_station = char_to_int(char_selection);
            }
            else if (!autosend)
            {
                rTB = char_to_int(char_selection);
            }

            if (selected_station < 0)
            {
                send = false;
            }
            else if (selected_station < kMax_decoded_messages)
            {
                send = true;
            }

        } //end of while loop

        if (!autosend)
        {
            if (rTB == 0)
            {
                sendChoices.call_cq = true;
            }
            else if (rTB == 1)
            {
                sendChoices.send_grid = true;
            }
            else if (rTB == 2)
            {
                sendChoices.send_snr = true;
            }
            else if (rTB == 3)
            {
                sendChoices.send_Rsnr = true;
            }
            else if (rTB == 4)
            {
                sendChoices.send_RRR = true;
            }
            else if (rTB == 5)
            {
                sendChoices.send_RR73 = true;
            }
            else if (rTB == 6)
            {
                sendChoices.send_73 = true;
            }
        }
        //need to make autosend automatically select the station
        if (send)
        {
            bool same_station = strcmp(CurrentStation.station_callsign,message_list[selected_station].station_callsign);
            CurrentStation = message_list[selected_station];
            update_current_station_display(CurrentStation, same_station);
            printf("CurrentStation set to %d\n",selected_station);
            printf("can callsign be seen? %s\n", CurrentStation.station_callsign);
        }

        //reset message_list
        printf("clearing %d bytes of message_list\n", sizeof(message_list));
        memset(message_list, 0, sizeof(message_list));
        printf("done a loop!\n");
    }
    return (0);
}