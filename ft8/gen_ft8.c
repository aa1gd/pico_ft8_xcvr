#include "gen_ft8.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "pack.h"
#include "encode.h"
#include "constants.h"

//recreated 9/20/21

//created 9/21/21
//CQ calls will always be manually generated
void manual_gen_message(char message[], message_info Station, UserSendSelection stype, char *myCall, char *myGrid){
    char snr_in_string[4];
    //itoa(Station.self_rx_snr, snr_in_string, 10);
    sprintf(snr_in_string, "%d", Station.self_rx_snr);

    if (stype.call_cq){
        strcat(message, "CQ ");
        strcat(message, myCall);
        strcat(message, " ");
        strcat(message, myGrid);
    } else if(stype.send_grid){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message, " ");
        strcat(message, myGrid);
    } else if(stype.send_snr){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message, " ");
        strcat(message, snr_in_string);
    } else if(stype.send_Rsnr){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message," R");
        strcat(message, snr_in_string);
    } else if(stype.send_RRR){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message," RRR");
    } else if(stype.send_RR73){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message," RR73");
    } else if(stype.send_73){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message," 73");
    }
}

void auto_gen_message(char message[], message_info Station, char *myCall, char *myGrid){

    //should make it so if it's not addressed to you, won't respond
    char snr_in_string[14];
    //itoa(Station.self_rx_snr, snr_in_string, 10);
    //this sprintf is making strings from numbers, but isn't making the right strings...
    sprintf(snr_in_string, "%d", Station.self_rx_snr);

    if (Station.type_cq){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message, " ");
        strcat(message, myGrid);
    }

    else if (Station.type_grid){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message," ");
        strcat(message, snr_in_string);

    }

    //need to distinguish between Rsnr (recieved) and snr
    //if Rsnr is recieved send out an RRR or RR73
    else if (Station.type_snr){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message," R");
        strcat(message, snr_in_string);
    }

    else if (Station.type_RRR){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message," 73");
        //set global variable send to false. after this, we're done sending
    }

    else if (Station.type_73){
        strcat(message, Station.station_callsign);
        strcat(message, " ");
        strcat(message, myCall);
        strcat(message," 73");
        //set global variable send to false. after this, we're done sending

    }
}

void generate_ft8(char message[], uint8_t tone_sequence[])
{
    //int message_length = strlen(message);
    // First, pack the text data into binary message
    uint8_t packed[FT8_LDPC_K_BYTES];
    int rc = pack77(message, packed);

    if (rc < 0)
    {
        printf("Cannot parse message!\n");
        printf("RC = %d\n", rc);
    }

    printf("Packed data: ");
    for (int j = 0; j < 10; ++j)
    {
        printf("%02x ", packed[j]);
    }
    printf("\n");

    int num_tones = FT8_NN;

    // Second, encode the binary message as a sequence of FSK tones

    genft8(packed, tone_sequence);

    printf("FSK tones: ");
    for (int j = 0; j < num_tones; ++j)
    {
        printf("%d", tone_sequence[j]);
    }
    printf("\n");

    return;
}