#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "decode_ft8.h"

typedef struct
{
    bool call_cq;
    bool send_grid;
    bool send_snr;
    bool send_Rsnr;
    bool send_RRR;
    bool send_RR73;
    bool send_73;

} UserSendSelection;

void manual_gen_message(char message[], message_info Station, UserSendSelection stype, char *myCall, char *myGrid);

void auto_gen_message(char message[], message_info Station, char *myCall, char *myGrid);

void generate_ft8(char message[], uint8_t tone_sequence[]);