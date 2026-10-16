/* Host converter adapter for the preserved Genesis Plus GX Musashi core.
 * Execute original IP/FMWR bytes; collect MMIO writes with the core's actual
 * cycle counter. The separately licensed core is not a game dependency. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "m68kconf.h"
#include "m68k.h"

typedef struct { uint32_t frame,master; uint8_t port,data; } probe_event;
typedef struct { uint32_t frame,address,size,value; } probe_drum;
static uint8_t *memory;
static probe_event *events;
static probe_drum *drums;
static probe_drum *signals;
static unsigned count,capacity,drum_count,drum_capacity,frame;
static unsigned signal_count,signal_capacity;
static unsigned busy_until,status_reads,busy_reads,instructions;
static int failure,refresh_enabled;

int vdp_68k_irq_ack(int level){(void)level;return M68K_INT_ACK_AUTOVECTOR;}
static unsigned raw8(unsigned address){return memory[(address&0xffffff)^1];}
static void put8(unsigned address,unsigned data){memory[(address&0xffffff)^1]=(uint8_t)data;}
static void append_event(unsigned port,unsigned data){
    if(count==capacity){
        unsigned next=capacity?capacity*2:4096;
        void *p=realloc(events,next*sizeof(*events));
        if(!p){failure=1;return;}events=p;capacity=next;
    }
    events[count++]=(probe_event){frame,(unsigned)m68k.cycles,(uint8_t)port,(uint8_t)data};
}
static void append_drum(unsigned address,unsigned data){
    if(drum_count==drum_capacity){
        unsigned next=drum_capacity?drum_capacity*2:256;
        void *p=realloc(drums,next*sizeof(*drums));
        if(!p){failure=2;return;}drums=p;drum_capacity=next;
    }
    drums[drum_count++]=(probe_drum){frame,address,1,data&255};
}
static void append_signal(unsigned address,unsigned data){
    if(signal_count==signal_capacity){
        unsigned next=signal_capacity?signal_capacity*2:64;
        void *p=realloc(signals,next*sizeof(*signals));
        if(!p){failure=4;return;}signals=p;signal_capacity=next;
    }
    /* Main writes the source completion sequence to communication byte1E.
     * SUB DF52 consumes bit6 (effect), bit7 (music) and the low sequence.
     * Observe it separately so PCM command records retain their format. */
    signals[signal_count++]=(probe_drum){frame,address,1,data&255};
}
static unsigned io_read8(unsigned address){
    /* mem68k.c z80_read_byte: one 68000 cycle of Z80 bus latency. */
    if((address>>16)==0xa0)m68k.cycles+=7;
    if(address==0xa11100)return 0;
    if(address==0xa04000 || address==0xa04002){
        status_reads++;
        if(address==0xa04000 && (unsigned)m68k.cycles<busy_until){busy_reads++;return 0x80;}
        return 0;
    }
    return raw8(address);
}
static unsigned io_read16(unsigned address){
    if((address>>16)==0xa0){unsigned value=io_read8(address);return value|(value<<8);}
    return io_read8(address)<<8|io_read8(address+1);
}
static void io_write8(unsigned address,unsigned data){
    /* mem68k.c z80_write_byte uses the same bus latency. */
    if((address>>16)==0xa0)m68k.cycles+=7;
    put8(address,data);
    if(address>=0xa04000 && address<=0xa04003){
        append_event(address-0xa04000,data);
        if(address&1)busy_until=(((unsigned)m68k.cycles+41)/42+32)*42;
    }else if(address==0xc00011)append_event(4,data);
    else if(address>=0xa1201a && address<=0xa1201d)append_drum(address,data);
    else if(address==0xa1201e)append_signal(address,data);
}
static void io_write16(unsigned address,unsigned data){
    io_write8(address,data>>8);
    if((address>>16)!=0xa0)io_write8(address+1,data);
}

int fm_probe_init(const uint8_t *ip,unsigned ip_bytes,const uint8_t *fm,unsigned fm_bytes,int refresh){
    free(memory);memory=calloc(1,1u<<24);if(!memory)return 0;
    count=drum_count=signal_count=status_reads=busy_reads=instructions=frame=busy_until=0;failure=0;
    refresh_enabled=refresh;
    memset(&m68k,0,sizeof(m68k));m68k_init();
    for(unsigned page=0;page<256;page++)m68k.memory_map[page].base=memory+(page<<16);
    const unsigned io_pages[]={0xa0,0xa1,0xc0};
    for(unsigned i=0;i<3;i++){
        cpu_memory_map *map=&m68k.memory_map[io_pages[i]];
        map->read8=io_read8;map->read16=io_read16;
        map->write8=io_write8;map->write16=io_write16;
    }
    if(ip_bytes>0xfa00 || fm_bytes>0x10000)return 0;
    for(unsigned i=0;i<ip_bytes;i++)put8(0xff0600+i,ip[i]);
    for(unsigned i=0;i<fm_bytes;i++)put8(0x200000+i,fm[i]);
    m68k.refresh_cycles=refresh?128*7:INT_MAX;
    return 1;
}
void fm_probe_frame(unsigned value){
    frame=value;m68k.cycles=0;busy_until=0;
    m68k.refresh_cycles=refresh_enabled?128*7:INT_MAX;
}
void fm_probe_reg(unsigned reg,unsigned value){m68k_set_reg((m68k_register_t)reg,value);}
void fm_probe_write(unsigned address,const uint8_t *data,unsigned size){
    for(unsigned i=0;i<size;i++)put8(address+i,data[i]);
}
void fm_probe_read(unsigned address,uint8_t *data,unsigned size){
    for(unsigned i=0;i<size;i++)data[i]=(uint8_t)raw8(address+i);
}
int fm_probe_execute(unsigned start){
    m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_A7,0xff7000);
    put8(0xff7000,0);put8(0xff7001,1);put8(0xff7002,0xff);put8(0xff7003,0xfc);
    m68k_set_reg(M68K_REG_PC,start);
    for(unsigned n=0;n<2000000;n++){
        if((m68k_get_reg(M68K_REG_PC)&0xffffff)==0x1fffc)return failure?0:1;
        if(m68k.stopped || failure)return 0;
        m68k_run((unsigned)m68k.cycles+1);instructions++;
    }
    failure=3;return 0;
}
unsigned fm_probe_info(unsigned item){
    switch(item){
    case 0:return count;case 1:return drum_count;case 2:return status_reads;
    case 3:return busy_reads;case 4:return instructions;case 5:return (unsigned)m68k.cycles;
    case 6:return m68k_get_reg(M68K_REG_PC);case 7:return failure;
    case 8:return signal_count;default:return 0;
    }
}
const probe_event *fm_probe_events(void){return events;}
const probe_drum *fm_probe_drums(void){return drums;}
const probe_drum *fm_probe_signals(void){return signals;}
