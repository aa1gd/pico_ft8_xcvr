#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "unpack.h"
#include "ldpc.h"
#include "decode.h"
#include "constants.h"
#include "crc.h"

#include "..\fft/kiss_fftr.h"

#include "decode_ft8.h"
#include "../util/rx_ft8.h"
#include "pico/multicore.h"

//#include "hardware/timer.h" //won't be needed once delay is replaced with interrupt

const float fft_norm = 2.0f / nfft;

//AA1GD-added array of structures to store info in decoded messages 8/22/2021

//setup fft freq output power, which will be accessible by both cores
//maybe should be put in decode_ft8.c?
uint8_t mag_power[num_blocks * kFreq_osr * kTime_osr * num_bins] = {0};
waterfall_t power = {
    .num_blocks = num_blocks,
    .num_bins = num_bins,
    .time_osr = kTime_osr,
    .freq_osr = kFreq_osr,
    .mag = mag_power};

volatile int offset = 0;
//volatile float max_mag = -120.0f;

kiss_fftr_cfg fft_cfg;

float window[nfft]; //I wonder if static will work here

float hann_i(int i, int N)
{
    float x = sinf((float)3.1416 * i / N); //replaced M_PI with a float
    return x * x;
}

void make_window(void){
    const int len_window = (int) (1.8f * block_size); // hand-picked and optimized
    for (int i = 0; i < nfft; ++i)
    {
        window[i] = (i < len_window) ? hann_i(i, len_window) : 0;
    }

}

static float max2(float a, float b)
{
    return (a >= b) ? a : b;
}

// Compute FFT magnitudes (log power) for each timeslot in the signal

void inc_extract_power(int16_t signal[])
{   
    
    // Loop over two possible time offsets (0 and block_size/2)
    for (int time_sub = 0; time_sub < power.time_osr; ++time_sub)
    {
        kiss_fft_scalar timedata[nfft];
        kiss_fft_cpx freqdata[nfft / 2 + 1];
        float mag_db[nfft / 2 + 1];

        //printf("same sample in decode: %d\n", signal[420]); //this works
        // Extract windowed signal block
        for (int pos = 0; pos < nfft; ++pos)
        {
            //maybe I just need to convert back to a float here? added a divide by 32768.0 Sept 19 2021
            //Couldn't get kiss_fft to work with int16_t. Dividing by 2048 as ADC gives 12 bit readings Oct. 16 2021
            //changing window changed... something. Trying without window (just 1)
            //timedata[pos] = window[pos] * signal[(time_sub * subblock_size) + pos] / 2048.0f;
            timedata[pos] = signal[(time_sub * subblock_size) + pos] / 2048.0f;
        }
        //printf("right before kiss_fftr\n");
        kiss_fftr(fft_cfg, timedata, freqdata);
        //printf("right after kiss_fftr\n");

        // Compute log magnitude in decibels
        for (int idx_bin = 0; idx_bin < nfft / 2 + 1; ++idx_bin)
        {
            float mag2 = (freqdata[idx_bin].i * freqdata[idx_bin].i) + (freqdata[idx_bin].r * freqdata[idx_bin].r);
            mag_db[idx_bin] = 10.0f * log10f(1E-12f + mag2 * fft_norm * fft_norm);
        }
        
        //printf("computed log magnitude\n");
        // Loop over two possible frequency bin offsets (for averaging)
        for (int freq_sub = 0; freq_sub < power.freq_osr; ++freq_sub)
        {
            for (int pos = 0; pos < power.num_bins; ++pos)
            {
                
                //printf("ith loop of pos is: %d\n",pos);
                float db = mag_db[pos * power.freq_osr + freq_sub];
                // Scale decibels to unsigned 8-bit range and clamp the value
                // Range 0-240 covers -120..0 dB in 0.5 dB steps

                int scaled = (int)(2 * db + 240);
                power.mag[offset] = (scaled < 0) ? 0 : ((scaled > 255) ? 255 : scaled);
                power.mag[offset] = scaled;
                offset++;
                //printf("offset: %d\n", *offset);
                
            }
        }
    }
    //printf("the counter offset:%d \n",offset);
    //printf("finished an extraction\n");
    
    return;
}

// Sept. 30, 2021 this didn't work out... the pointers to power were getting too confusing and unneccesary
// Oct. 1, 2021 trying to implement again
// the signal needs to be 3/2 (1.5) times as long as nfft
// Oct. 2, 2021 got time_osr = 1 incremental decoding working
void inc_collect_power(){
    size_t fft_work_size;
    kiss_fftr_alloc(nfft, 0, 0, &fft_work_size);

    printf("Sample rate %d Hz, %d blocks, %d bins\n", sample_rate, num_blocks, num_bins);
    printf("This is size of array mag_power in bytes: %d\n", num_blocks * kFreq_osr * kTime_osr * num_bins);

    void *fft_work = malloc(fft_work_size);
    fft_cfg = kiss_fftr_alloc(nfft, 0, fft_work, &fft_work_size);
    printf("starting incremental collection\n");

    //PASS IDX_BLOCK THROUGH THE FIFO-this may help with the offset
    for (uint idx_block = 0; idx_block < num_blocks; idx_block++){
        collect_adc();
        multicore_fifo_push_blocking(idx_block);
    }

    //may want to wait or get a message back from core 1 before memory is freed
    busy_wait_ms(160); //waits a bit for fft to finish. May want to replace with a confirmation back from core 1
    free(fft_work);
    printf("done collecting power\n");
    printf("resetting offset and max mag\n");
    offset = 0;
    return;
}

int decode_ft8(message_info message_list[])
{
    // Find top candidates by Costas sync score and localize them in time and frequency
    candidate_t candidate_list[kMax_candidates];
    int num_candidates = find_sync(&power, kMax_candidates, candidate_list, kMin_score);

    // Hash table for decoded messages (to check for duplicates)
    int num_decoded = 0;
    message_t decoded[kMax_decoded_messages];
    message_t *decoded_hashtable[kMax_decoded_messages];

    //if using calc_noise type 1 or 2 use the below two funcions. type 3 goes right before calc_snr
    int noise_avg = calc_noise(&power,NULL);
    printf("Noise average: %d \n", noise_avg);

    // Initialize hash table pointers
    for (int i = 0; i < kMax_decoded_messages; ++i)
    {
        decoded_hashtable[i] = NULL;
    }

    // Go over candidates and attempt to decode messages
    for (int idx = 0; idx < num_candidates; ++idx)
    {
        //AA1GD added to try correctly stop program when decoded>kMax_decoded_messages
        //printf("num decoded %d \n",num_decoded);
        if(num_decoded >= kMax_decoded_messages){
            printf("decoded more than kMax_decoded_messages\n");
            printf("Decoded %d messages and force ended\n", num_decoded);
            return(num_decoded);
        }

        const candidate_t *cand = &candidate_list[idx];
        if (cand->score < kMin_score)
            continue;

        float freq_hz = (cand->freq_offset + (float)cand->freq_sub / kFreq_osr) * kFSK_dev;
        float time_sec = (cand->time_offset + (float)cand->time_sub / kTime_osr) / kFSK_dev;

        message_t message;
        decode_status_t status;
        
        if (!decode(&power, cand, &message, kLDPC_iterations, &status))
        {
            if (status.ldpc_errors > 0)
            {
                //printf("LDPC decode: %d errors\n", status.ldpc_errors);
            }
            else if (status.crc_calculated != status.crc_extracted)
            {
                //printf("CRC mismatch!\n");
            }
            else if (status.unpack_status != 0)
            {
                //printf("Error while unpacking!\n");
            }
            continue;
        }

        //printf("Checking hash table for %4.1fs / %4.1fHz [%d]...\n", time_sec, freq_hz, cand->score);
        int idx_hash = message.hash % kMax_decoded_messages;
        bool found_empty_slot = false;
        bool found_duplicate = false;
        do
        {
            if (decoded_hashtable[idx_hash] == NULL)
            {
                //printf("Found an empty slot\n");
                found_empty_slot = true;
            }
            else if ((decoded_hashtable[idx_hash]->hash == message.hash) && (0 == strcmp(decoded_hashtable[idx_hash]->text, message.text)))
            {
                //printf("Found a duplicate [%s]\n", message.text);
                found_duplicate = true;
            }
            else
            {
                //printf("Hash table clash!\n");
                // Move on to check the next entry in hash table
                idx_hash = (idx_hash + 1) % kMax_decoded_messages;
            }
        } while (!found_empty_slot && !found_duplicate);

        if (found_empty_slot)
        {
            // Fill the empty hashtable slot
            memcpy(&decoded[idx_hash], &message, sizeof(message));
            decoded_hashtable[idx_hash] = &decoded[idx_hash];

            int snr = calc_snr(&power,cand,noise_avg);

            //printf("%x     %3d %+4.2f %4d ~  %s\n", num_decoded, cand->score, time_sec, freq_hz, message.text);
            //printf("about to ");
            printf("%x   %3d  %+3.1f %4d ~  %s\n", num_decoded, snr, time_sec, (int) freq_hz, message.text);

            //printf("estimated snr: %d \n \n", snr);

            //AA1GD-add message info to the global variable message_list
            //message_list[num_decoded].self_rx_snr = snr;
            message_list[num_decoded].self_rx_snr = snr;
            message_list[num_decoded].af_frequency = (uint16_t) freq_hz;
            message_list[num_decoded].time_offset = time_sec;
            strcpy(message_list[num_decoded].full_text, message.text);
            
            ++num_decoded;
        }
    }
    printf("Decoded %d messages\n", num_decoded);

    return num_decoded;
}

//need to finish this. Sept. 19 2021 also need to fix snr linear regression
//should input global struct message_info current_station to compare current station
void identify_message_types(message_info message_list[], char *my_callsign){

    for (int i = 0; i < kMax_decoded_messages; i++){

        if(!(message_list[i].af_frequency)){ //checks if its empty
            return;
        }

        if (strstr(message_list[i].full_text, my_callsign)){
            message_list[i].addressed_to_me = true;
        }

        if (strstr(message_list[i].full_text, "CQ")){
            message_list[i].type_cq = true;
        }

        if (strstr(message_list[i].full_text, "RRR")){
            message_list[i].type_RRR = true;
        }

        if (strstr(message_list[i].full_text, "73")){
            message_list[i].type_73 = true;
        }

        char message_buffer[25];
        strcpy(message_buffer, message_list[i].full_text);
        const char delim[2] = " ";

        char *first_word;
        first_word = strtok(message_buffer,delim);
        //printf("%d first %s\n",i, first_word);

        char *second_word;
        second_word = strtok(NULL,delim);
        //printf("%d second %s\n",i, second_word);

        char *third_word;
        third_word = strtok(NULL,delim);
        //printf("%d third %s\n",i, third_word);

        //fourth word supports CQ extra tags like DX, POTA, QRP
        char *fourth_word;
        fourth_word = strtok(NULL,delim);
        //printf("%d fourth %s\n",i, fourth_word);

        if (strlen(second_word) > 3 && !fourth_word){
            strcpy(message_list[i].station_callsign, second_word);
        } else if(strlen(third_word) > 3 && fourth_word){
            strcpy(message_list[i].station_callsign, third_word);
        }
        
        if(!third_word){
            third_word = "N/A";
        }
        
        if (message_list[i].type_cq){
            if(!fourth_word){
                strcpy(message_list[i].grid_square, third_word);
            } else{
                strcpy(message_list[i].grid_square, fourth_word);
            }
        }

        else if (strlen(third_word)==4 && !strchr(third_word, '-') && !strchr(third_word, '+') && !strcmp(third_word,"RR73")){
            message_list[i].type_grid = true;
            strcpy(message_list[i].grid_square, third_word);
        }

        else if (strchr(third_word, '-') || strchr(third_word, '+')){
            if (strchr(third_word, 'R')){
                message_list[i].type_Rsnr = true;
            }
            else {message_list[i].type_snr = true;}
            
            strcpy(message_list[i].snr_report, third_word);
        }

        //printf("station callsign: %s grid square: %s snr report: %s\n\n",message_list[i].station_callsign, message_list[i].grid_square, message_list[i].snr_report);
    }
    return;
}