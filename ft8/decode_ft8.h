#pragma once
#include "decode.h"
#include "../fft/kiss_fftr.h"

#define kMin_score 10 // Minimum sync score threshold for candidates
#define kMax_candidates 30 // Original was 120
#define kLDPC_iterations 10 // Original was 20

#define kMax_decoded_messages 14 //was 50, change to 14 since there's 14 buttons on the 4x4 membrane keyboard

#define kFreq_osr 1 //both default 2
#define kTime_osr 1

#define kFSK_dev 6.25 // tone deviation in Hz and symbol rate

#define sample_rate 6000

#define decoding_time 12.8

enum {num_bins = (int)(sample_rate / (2 * kFSK_dev))}; // number bins of FSK tone width that the spectrum can be divided into
enum {block_size = (int)(sample_rate / kFSK_dev)};     // samples corresponding to one FSK symbol
enum {subblock_size = block_size / kTime_osr};
enum {nfft = block_size * kFreq_osr};
enum {num_blocks = (int) (decoding_time * kFSK_dev)};

enum {num_samples_processed = nfft * (1 + kTime_osr) / 2};


//extern uint8_t mag_power[num_blocks * kFreq_osr * kTime_osr * num_bins];
extern waterfall_t power;

typedef struct
{
    int8_t self_rx_snr; //(should be -24 to 30 db)
    uint16_t af_frequency;
    float time_offset;
    char full_text[19]; // was 25
    bool addressed_to_me;
    bool type_cq;
    bool type_grid;
    bool type_snr;
    bool type_Rsnr;
    bool type_RRR;
    bool type_73;
    char station_callsign[7]; //do I need 7 instead of 6? For some reason my testing files have some 7-character callsigns
    char grid_square[4];
    char snr_report[4];
}message_info;

float hann_i(int i, int N);

void make_window(void);

static float max2(float a, float b);

// Compute FFT magnitudes (log power) for each timeslot in the signal
void inc_extract_power(int16_t signal[]);

void inc_collect_power();

int decode_ft8(message_info message_list[]);

void identify_message_types(message_info message_list[], char *my_callsign); 