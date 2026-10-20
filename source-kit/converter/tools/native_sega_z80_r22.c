/* Isolated original BIOS/security Z80 sound-device probe.
 * No recording, saved RAM, framebuffer or copied game implementation input.
 * CPU dependency: floooh/chips z80.h, retained under its original zlib license.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define CHIPS_IMPL
#include "../third_party/chips-z80/z80.h"
typedef struct {uint64_t clock;uint8_t port,data;} sound_event;
int native_sega_trace(const uint8_t *bios,size_t bios_size,
        const uint8_t *security,size_t security_size,unsigned frames,
        sound_event *events,size_t capacity,uint8_t *final_ram){
    if(bios_size!=131072 || security_size!=1536 || frames>1000)return -1;
    uint8_t ram[8192]={0};
    /* BIOS0FC0 initial load, followed by68C4's security instrument table. */
    memcpy(ram,bios+0xf000,0xca0);
    memcpy(ram+0xd00,bios+0xfca0,0x17a);
    memcpy(ram+0x1100,bios+0xfe1a,0x4ba);
    memcpy(ram+0x1100,security+0x4a4,0xe0);
    z80_t cpu;uint64_t pins=z80_init(&cpu),last_write=0;
    uint32_t bank=0;size_t count=0;
    uint64_t end=(uint64_t)frames*896040/15;
    for(uint64_t tick=0;tick<end;tick++){
        /* BIOS69CA requests the source SEGA voice after60+57 VBlanks. */
        if(tick==(uint64_t)117*896040/15)ram[0x1c0a]=0x81;
        uint64_t phase=tick*15%896040;
        if(phase>=224*3420 && phase<225*3420)pins|=Z80_INT;
        else pins&=~Z80_INT;
        pins=z80_tick(&cpu,pins);
        if(pins&Z80_MREQ){
            uint16_t address=Z80_GET_ADDR(pins);
            if(pins&Z80_RD){
                uint8_t value=0;
                if(address<0x4000)value=ram[address&8191];
                else if(address<0x6000)value=0;
                else if(address>=0x8000){
                    uint32_t at=bank|(address&0x7fff);
                    if(at<bios_size)value=bios[at];
                    else return -4;
                }
                Z80_SET_DATA(pins,value);
            }else if((pins&Z80_WR) && tick!=last_write){
                last_write=tick;uint8_t value=Z80_GET_DATA(pins);
                if(address<0x4000)ram[address&8191]=value;
                else if(address>=0x4000 && address<0x6000){
                    if(count>=capacity)return -2;
                    events[count++]=(sound_event){tick*15/42,(uint8_t)(address&3),value};
                }else if(address>=0x6000 && address<0x6100){
                    bank=((bank>>1)|((value&1)<<23))&0xff8000;
                }else if(address==0x7f11){
                    if(count>=capacity)return -2;
                    events[count++]=(sound_event){tick*15/42,4,value};
                }
            }
        }else if(pins&Z80_IORQ){
            if(pins&Z80_RD)Z80_SET_DATA(pins,0xff);
        }
    }
    if(final_ram)memcpy(final_ram,ram,sizeof(ram));
    return (int)count;
}
