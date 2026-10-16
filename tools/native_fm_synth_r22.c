/* Port-owned source-device renderer using the separately licensed Nuked-OPN2.
 * Input: original sound-driver register events. No recording/movie input.
 * FM3F has no active PSG or SCD drum writes in the isolated native trace.
 */
#include <stdint.h>
#include <stddef.h>
#include "../third_party/Nuked-OPN2/ym3438.h"
typedef struct { uint64_t clock;uint8_t port,data; } fm_event;
static int16_t clip(int32_t value){return value>32767?32767:value<-32768?-32768:value;}
int native_fm_render(const fm_event *events,size_t count,int16_t *output,size_t samples){
    ym3438_t chip;OPN2_SetChipType(ym3438_mode_ym2612);OPN2_Reset(&chip);
    int16_t history[24][2]={{0}};unsigned index=0;int32_t sums[2]={0};
    uint64_t clock=0,ready=0;size_t event=0;
    int32_t previous[2]={0};int64_t filtered[2]={0};
    for(size_t sample=0;sample<samples;sample++){
        uint64_t end=(uint64_t)(sample+1)*53693175/(42*32000);
        while(clock<end){
            if(event<count && events[event].clock<=clock && clock>=ready){
                const fm_event *e=&events[event++];
                if(e->port==4){if(e->data!=0x9f && e->data!=0xbf && e->data!=0xdf && e->data!=0xff)return -2;}
                else if(e->port<4){OPN2_Write(&chip,e->port,e->data);ready=clock+((e->port&1)?36:4);}
                else return -3;
            }
            int16_t pins[2];OPN2_Clock(&chip,pins);
            for(unsigned channel=0;channel<2;channel++){
                sums[channel]+=pins[channel]-history[index][channel];history[index][channel]=pins[channel];
            }
            index=(index+1)%24;clock++;
        }
        /* AC-coupled output: the multiplexed DAC pedestal also changes with
         * panning. A20Hz pole removes DC while retaining low musical notes. */
        for(unsigned channel=0;channel<2;channel++){
            int32_t value=sums[channel]*8;
            filtered[channel]=(int64_t)(value-previous[channel])*65536+filtered[channel]*65279/65536;
            previous[channel]=value;
            output[sample*2+channel]=clip((int32_t)(filtered[channel]/65536));
        }
    }
    return (int)event;
}
/* Split the actual multiplexed YM2612 pins, retaining each channel's DAC
 * pedestal, panning and filter. No register write or operator is removed.
 * Summing these buses agrees with the ordinary output to integer rounding. */
int native_fm_render_channels(const fm_event *events,size_t count,int16_t **output,size_t samples){
    ym3438_t chip;OPN2_SetChipType(ym3438_mode_ym2612);OPN2_Reset(&chip);
    int16_t history[24][2]={{0}};unsigned index=0;
    int32_t sums[6][2]={{0}},previous[6][2]={{0}};int64_t filtered[6][2]={{0}};
    static const unsigned bus[6]={1,5,3,0,4,2};
    uint64_t clock=0,ready=0;size_t event=0;
    for(size_t sample=0;sample<samples;sample++){
        uint64_t end=(uint64_t)(sample+1)*53693175/(42*32000);
        while(clock<end){
            if(event<count && events[event].clock<=clock && clock>=ready){
                const fm_event *e=&events[event++];
                if(e->port==4){if(e->data!=0x9f && e->data!=0xbf && e->data!=0xdf && e->data!=0xff)return -2;}
                else if(e->port<4){OPN2_Write(&chip,e->port,e->data);ready=clock+((e->port&1)?36:4);}
                else return -3;
            }
            unsigned voice=bus[chip.cycles/4];
            int16_t pins[2];OPN2_Clock(&chip,pins);
            for(unsigned channel=0;channel<2;channel++){
                sums[voice][channel]+=pins[channel]-history[index][channel];history[index][channel]=pins[channel];
            }
            index=(index+1)%24;clock++;
        }
        for(unsigned voice=0;voice<6;voice++)for(unsigned channel=0;channel<2;channel++){
            int32_t value=sums[voice][channel]*8;
            filtered[voice][channel]=(int64_t)(value-previous[voice][channel])*65536+filtered[voice][channel]*65279/65536;
            previous[voice][channel]=value;
            output[voice][sample*2+channel]=clip((int32_t)(filtered[voice][channel]/65536));
        }
    }
    return (int)event;
}
