#include "decode.h"
#include "constants.h"
#include "crc.h"
#include "ldpc.h"
#include "unpack.h"

#include <stdbool.h>
#include <math.h>

#include <stdio.h>

#include "decode_ft8.h" // for the constants

/// Compute log likelihood log(p(1) / p(0)) of 174 message bits for later use in soft-decision LDPC decoding
/// @param[in] power Waterfall data collected during message slot
/// @param[in] cand Candidate to extract the message from
/// @param[in] code_map Symbol encoding map
/// @param[out] log174 Output of decoded log likelihoods for each of the 174 message bits
static void extract_likelihood(const waterfall_t *power, const candidate_t *cand, float *log174);

static float max2(float a, float b);
static float max4(float a, float b, float c, float d);
static void heapify_down(candidate_t heap[], int heap_size);
static void heapify_up(candidate_t heap[], int heap_size);
static void decode_symbol(const uint8_t *power, int bit_idx, float *log174);
//static void decode_multi_symbols(const uint8_t *power, int num_bins, int n_syms, int bit_idx, float *log174);

static int get_index(const waterfall_t *power, int block, int time_sub, int freq_sub, int bin)
{
    return ((((block * power->time_osr) + time_sub) * power->freq_osr + freq_sub) * power->num_bins) + bin;
}

int find_sync(const waterfall_t *power, int num_candidates, candidate_t heap[], int min_score)
{
    int heap_size = 0;
    int sym_stride = power->time_osr * power->freq_osr * power->num_bins;

    // Here we allow time offsets that exceed signal boundaries, as long as we still have all data bits.
    // I.e. we can afford to skip the first 7 or the last 7 Costas symbols, as long as we track how many
    // sync symbols we included in the score, so the score is averaged.
    for (int time_sub = 0; time_sub < power->time_osr; ++time_sub)
    {
        for (int freq_sub = 0; freq_sub < power->freq_osr; ++freq_sub)
        {
            for (int time_offset = -12; time_offset < 24; ++time_offset)
            {
                for (int freq_offset = 0; freq_offset + 7 < power->num_bins; ++freq_offset)
                {
                    int score = 0;
                    int num_average = 0;

                    // Compute average score over sync symbols (m+k = 0-7, 36-43, 72-79)
                    for (int m = 0; m <= 72; m += 36)
                    {
                        for (int k = 0; k < 7; ++k)
                        {
                            int block = time_offset + m + k;
                            // Check for time boundaries
                            if (block < 0)
                                continue;
                            if (block >= power->num_blocks)
                                break;

                            int offset = get_index(power, block, time_sub, freq_sub, freq_offset);
                            const uint8_t *p8 = power->mag + offset;

                            // Weighted difference between the expected and all other symbols
                            // Does not work as well as the alternative score below
                            // score += 8 * p8[kFT8_Costas_pattern[k]] -
                            //          p8[0] - p8[1] - p8[2] - p8[3] -
                            //          p8[4] - p8[5] - p8[6] - p8[7];
                            // ++num_average;

                            // Check only the neighbors of the expected symbol frequency- and time-wise
                            int sm = kFT8_Costas_pattern[k]; // Index of the expected bin
                            if (sm > 0)
                            {
                                // look at one frequency bin lower
                                score += p8[sm] - p8[sm - 1];
                                ++num_average;
                            }
                            if (sm < 7)
                            {
                                // look at one frequency bin higher
                                score += p8[sm] - p8[sm + 1];
                                ++num_average;
                            }
                            if ((k > 0) && (block > 0))
                            {
                                // look one symbol back in time
                                score += p8[sm] - p8[sm - sym_stride];
                                ++num_average;
                            }
                            if ((k < 6) && ((block + 1) < power->num_blocks))
                            {
                                // look one symbol forward in time
                                score += p8[sm] - p8[sm + sym_stride];
                                ++num_average;
                            }
                        }
                    }

                    if (num_average > 0)
                        score /= num_average;

                    if (score < min_score)
                        continue;

                    // If the heap is full AND the current candidate is better than
                    // the worst in the heap, we remove the worst and make space
                    if (heap_size == num_candidates && score > heap[0].score)
                    {
                        heap[0] = heap[heap_size - 1];
                        --heap_size;

                        heapify_down(heap, heap_size);
                    }

                    // If there's free space in the heap, we add the current candidate
                    if (heap_size < num_candidates)
                    {
                        heap[heap_size].score = score;
                        heap[heap_size].time_offset = time_offset;
                        heap[heap_size].freq_offset = freq_offset;
                        heap[heap_size].time_sub = time_sub;
                        heap[heap_size].freq_sub = freq_sub;
                        ++heap_size;

                        heapify_up(heap, heap_size);
                    }
                }
            }
        }
    }

    // Sort the candidates by sync strength - here we benefit from the heap structure
    int len_unsorted = heap_size;
    while (len_unsorted > 1)
    {
        candidate_t tmp = heap[len_unsorted - 1];
        heap[len_unsorted - 1] = heap[0];
        heap[0] = tmp;
        len_unsorted--;
        heapify_down(heap, len_unsorted);
    }

    return heap_size;
}

void extract_likelihood(const waterfall_t *power, const candidate_t *cand, float *log174)
{
    int sym_stride = power->time_osr * power->freq_osr * power->num_bins;
    int offset = get_index(power, cand->time_offset, cand->time_sub, cand->freq_sub, cand->freq_offset);

    // Go over FSK tones and skip Costas sync symbols
    const int n_syms = 1;
    const int n_bits = 3 * n_syms;
    const int n_tones = (1 << n_bits);
    for (int k = 0; k < FT8_ND; k += n_syms)
    {
        // Add either 7 or 14 extra symbols to account for sync
        int sym_idx = (k < FT8_ND / 2) ? (k + 7) : (k + 14);
        int bit_idx = 3 * k;

        int block = cand->time_offset + sym_idx;

        // Check for time boundaries
        if ((block < 0) || (block >= power->num_blocks))
        {
            log174[bit_idx] = 0;
        }
        else
        {
            // Pointer to 8 bins of the current symbol
            const uint8_t *ps = power->mag + offset + (sym_idx * sym_stride);

            decode_symbol(ps, bit_idx, log174);
        }
    }

    // Compute the variance of log174
    float sum = 0;
    float sum2 = 0;
    for (int i = 0; i < FT8_LDPC_N; ++i)
    {
        sum += log174[i];
        sum2 += log174[i] * log174[i];
    }
    float inv_n = 1.0f / FT8_LDPC_N;
    float variance = (sum2 - (sum * sum * inv_n)) * inv_n;

    // Normalize log174 distribution and scale it with experimentally found coefficient
    float norm_factor = sqrtf(24.0f / variance);
    for (int i = 0; i < FT8_LDPC_N; ++i)
    {
        log174[i] *= norm_factor;
    }
}

bool decode(const waterfall_t *power, const candidate_t *cand, message_t *message, int max_iterations, decode_status_t *status)
{
    float log174[FT8_LDPC_N]; // message bits encoded as likelihood
    extract_likelihood(power, cand, log174);

    uint8_t plain174[FT8_LDPC_N]; // message bits (0/1)
    bp_decode(log174, max_iterations, plain174, &status->ldpc_errors);
    // ldpc_decode(log174, max_iterations, plain174, &status->ldpc_errors);

    if (status->ldpc_errors > 0)
    {
        return false;
    }

    // Extract payload + CRC (first FT8_LDPC_K bits) packed into a byte array
    uint8_t a91[FT8_LDPC_K_BYTES];
    pack_bits(plain174, FT8_LDPC_K, a91);

    // Extract CRC and check it
    status->crc_extracted = extract_crc(a91);
    // [1]: 'The CRC is calculated on the source-encoded message, zero-extended from 77 to 82 bits.'
    a91[9] &= 0xF8;
    a91[10] &= 0x00;
    status->crc_calculated = ft8_crc(a91, 96 - 14);

    if (status->crc_extracted != status->crc_calculated)
    {
        return false;
    }

    status->unpack_status = unpack77(a91, message->text);

    if (status->unpack_status < 0)
    {
        return false;
    }

    // Reuse binary message CRC as hash value for the message
    message->hash = status->crc_extracted;

    return true;
}

static float max2(float a, float b)
{
    return (a >= b) ? a : b;
}

static float max4(float a, float b, float c, float d)
{
    return max2(max2(a, b), max2(c, d));
}

static void heapify_down(candidate_t heap[], int heap_size)
{
    // heapify from the root down
    int current = 0;
    while (true)
    {
        int largest = current;
        int left = 2 * current + 1;
        int right = left + 1;

        if (left < heap_size && heap[left].score < heap[largest].score)
        {
            largest = left;
        }
        if (right < heap_size && heap[right].score < heap[largest].score)
        {
            largest = right;
        }
        if (largest == current)
        {
            break;
        }

        candidate_t tmp = heap[largest];
        heap[largest] = heap[current];
        heap[current] = tmp;
        current = largest;
    }
}

static void heapify_up(candidate_t heap[], int heap_size)
{
    // heapify from the last node up
    int current = heap_size - 1;
    while (current > 0)
    {
        int parent = (current - 1) / 2;
        if (heap[current].score >= heap[parent].score)
        {
            break;
        }

        candidate_t tmp = heap[parent];
        heap[parent] = heap[current];
        heap[current] = tmp;
        current = parent;
    }
}

// Compute unnormalized log likelihood log(p(1) / p(0)) of 3 message bits (1 FSK symbol)
static void decode_symbol(const uint8_t *power, int bit_idx, float *log174)
{
    // Cleaned up code for the simple case of n_syms==1
    float s2[8];

    for (int j = 0; j < 8; ++j)
    {
        s2[j] = (float)power[kFT8_Gray_map[j]];
    }

    log174[bit_idx + 0] = max4(s2[4], s2[5], s2[6], s2[7]) - max4(s2[0], s2[1], s2[2], s2[3]);
    log174[bit_idx + 1] = max4(s2[2], s2[3], s2[6], s2[7]) - max4(s2[0], s2[1], s2[4], s2[5]);
    log174[bit_idx + 2] = max4(s2[1], s2[3], s2[5], s2[7]) - max4(s2[0], s2[2], s2[4], s2[6]);
}

//Added by AA1GD Aug. 23 2021
//I am unsure if the FFT magnitudes given between 0-256 are linear or logarithmic?
//They are log

int calc_noise(const waterfall_t *power, const candidate_t *cand){
    //other option is to sample the ~half second quiet before transmissions start
    // set noise_sample_type to 1 to sample single frequency, 2 for average over frequency range, 3 for the dead time before transmissions
    char noise_sample_type = 2; 

    long noise_sum = 0;
    int noise_sampling_freq = 250; //finds noise levels between (noise_sampling_freq, noise_sampling_freq+noise_sampling_freq_range)
    int noise_sampling_freq_range = 50; //a multiple of 6.25Hz

    int noise_sampling_start_block = 1; //applies to types 1,2,3

    int noise_avg;
    //we will start k at one since it seems the first block is zero

    if (noise_sample_type == 1){
        for(int k = noise_sampling_start_block; k<power->num_blocks; k++){
            noise_sum += power->mag[get_index(power,k,0,0,noise_sampling_freq/6.25)];
        }
        noise_avg = noise_sum / (power->num_blocks - noise_sampling_start_block);
    }

    if (noise_sample_type == 2){
        for(int i = noise_sampling_start_block; i<noise_sampling_freq_range/6.25; i++){

            for(int k = 0; k<power->num_blocks; k++){
                noise_sum += power->mag[get_index(power,k,0,0,i + noise_sampling_freq/6.25)];
            }
        }
        noise_avg = noise_sum / ((power->num_blocks - noise_sampling_start_block) * noise_sampling_freq_range/6.25);
    }
    
    //Samples noise before transmission. This may not be good for wav decoding because of AGC.
    //Has not been tested
    if (noise_sample_type == 3){
        int noise_sampling_end_block = cand->time_sub; //applies to type 3 only, stops sampling just before signal begins.
        //May need to subtract 1 although this is done in the for loop?
        for(int i = noise_sampling_start_block; i<noise_sampling_end_block; i++){

            for(int j = 0; j < 8; j++){
                noise_sum += power->mag[get_index(power,i,0,0,j + cand->freq_offset)];   
            }
        }
        noise_avg = noise_sum / (8*(noise_sampling_end_block-noise_sampling_start_block));
    }
    
    // if noise_avg is >255 or <0, something went wrong.

    return noise_avg;
}

//Very roughly estimates snr based on difference between noise and signal. Just did a linear regression to estimate
//relationship between (signal-noise) and actual snr calculated by WSJT-X
//The logarithmic magnitudes make it hard to do actual power calculations...
//some of these int variables can be replaced with u_int8
int calc_snr(const waterfall_t *power, const candidate_t *cand, int noise_avg){
    int sig_sum = 0;

    for (int i = 0; i < num_blocks; i++){
        //find the strongest of 8 tones for each of 79 symbol times
        int8_t max_tone_mag = 0;
        for (int k = 0; k < 8; k++){
            int8_t tone_mag = power->mag[get_index(power,cand->time_offset + i,cand->time_sub,cand->freq_sub,cand->freq_offset + k)];
            if(tone_mag > max_tone_mag){
                max_tone_mag = tone_mag;
            }
        }
        //printf("max tone: %d",max_tone_mag);
        sig_sum += max_tone_mag;
    }
    int sig_avg = sig_sum / 79;
    printf("sig avg: %d ",sig_avg);
    int sig_minus_noise = sig_avg-noise_avg;
    printf("sig-noise power: %d \n", sig_minus_noise);

    //int snr = round(0.427*sig_minus_noise - 33.7); //this linear regression can be improved with more testing
    int snr = (int)(0.427 * sig_minus_noise - 23.0);

    if (snr > 30){
        snr = 30;
    }
    if(snr < -24){
        snr = -24;
    }

    //printf("finished calcsnr\n");
    return snr;

}