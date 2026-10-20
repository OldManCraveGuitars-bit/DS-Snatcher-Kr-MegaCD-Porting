/* Development-only adapter for the preserved Genesis Plus GX Musashi core.
 * Execute original IP/FMWR bytes; collect MMIO writes with the core's actual
 * cycle counter. This separately licensed core is used by the private4C game integration. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "m68kconf.h"
#include "m68k.h"

typedef struct { uint32_t frame,master; uint8_t port,data; } probe_event;
typedef struct { uint32_t frame,address,size,value; } probe_drum;
static uint8_t *memory;
static uint8_t io_a1[64],io_c0[32];
static unsigned bad_address;
static uint8_t *address_byte(unsigned address){
 address&=0xffffff;unsigned page=address>>16,low=(address&65535)^1;
 if(page==0xff)return memory+low;
 if(page==0x20)return memory+65536+low;
 if(page==0xa0)return memory+131072+low;
 if(page==0xa1 && address>=0xa12000 && address<0xa12040)return io_a1+((address&63)^1);
 if(page==0xa1 && (address==0xa11100 || address==0xa11101))return io_a1+((address&1)^1);
 if(page==0xc0 && (address&65535)<32)return io_c0+low;
 bad_address=address;return memory+196608+low;
}

static probe_event *events;
static probe_drum *drums;
static unsigned count,capacity,drum_count,drum_capacity,frame;
static unsigned busy_until,status_reads,busy_reads,instructions;
static int failure,refresh_enabled;

int vdp_68k_irq_ack(int level){(void)level;return M68K_INT_ACK_AUTOVECTOR;}
static unsigned raw8(unsigned address){return *address_byte(address);}
static void put8(unsigned address,unsigned data){*address_byte(address)=(uint8_t)data;}
static void append_event(unsigned port,unsigned data){
    if(count==capacity){
        unsigned next=capacity?capacity*2:512;if(next>4096){failure=4;return;}
        void *p=realloc(events,next*sizeof(*events));
        if(!p){failure=1;return;}events=p;capacity=next;
    }
    events[count++]=(probe_event){frame,(unsigned)m68k.cycles,(uint8_t)port,(uint8_t)data};
}
static void append_drum(unsigned address,unsigned data){
    if(drum_count==drum_capacity){
        unsigned next=drum_capacity?drum_capacity*2:32;if(next>256){failure=5;return;}
        void *p=realloc(drums,next*sizeof(*drums));
        if(!p){failure=2;return;}drums=p;drum_capacity=next;
    }
    drums[drum_count++]=(probe_drum){frame,address,1,data&255};
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
}
static void io_write16(unsigned address,unsigned data){
    io_write8(address,data>>8);
    if((address>>16)!=0xa0)io_write8(address+1,data);
}

int fm_probe_init(const uint8_t *ip,unsigned ip_bytes,const uint8_t *fm,unsigned fm_bytes,int refresh){
    free(memory);memory=calloc(1,4u<<16);if(!memory)return 0;
    bad_address=0;memset(io_a1,0,sizeof(io_a1));memset(io_c0,0,sizeof(io_c0));
    count=drum_count=status_reads=busy_reads=instructions=frame=busy_until=0;failure=0;
    refresh_enabled=refresh;
    memset(m68k_memory_map,0,sizeof(m68k_memory_map));memset(&m68k,0,sizeof(m68k));m68k_init();
    for(unsigned page=0;page<256;page++)m68k_memory_map[page].base=memory+196608;
    m68k_memory_map[0xff].base=memory;
    m68k_memory_map[0x20].base=memory+65536;
    m68k_memory_map[0xa0].base=memory+131072;
    const unsigned io_pages[]={0xa0,0xa1,0xc0};
    for(unsigned i=0;i<3;i++){
        cpu_memory_map *map=&m68k_memory_map[io_pages[i]];
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
    extern unsigned fm_compact_steps;
    fm_compact_steps=0;m68k_run(0x7fffffffu);instructions+=fm_compact_steps;
    if((m68k_get_reg(M68K_REG_PC)&0xffffff)==0x1fffc && !m68k.stopped && !bad_address)
        return failure?0:1;
    failure=3;return 0;
}
unsigned fm_probe_info(unsigned item){
    switch(item){
    case 0:return count;case 1:return drum_count;case 2:return status_reads;
    case 3:return busy_reads;case 4:return instructions;case 5:return (unsigned)m68k.cycles;
    case 6:return m68k_get_reg(M68K_REG_PC);case 7:return failure;default:return 0;
    }
}
const probe_event *fm_probe_events(void){return events;}
const probe_drum *fm_probe_drums(void){return drums;}

unsigned fm_probe_bad_address(void){return bad_address;}

/* Each source tick owns its writes until the caller consumes them. */
void fm_probe_clear_events(void){count=drum_count=0;}
static unsigned fnv_byte(unsigned hash,unsigned value){return (hash^(value&255))*16777619u;}
static unsigned fnv_word(unsigned hash,unsigned value){
 for(unsigned i=0;i<4;i++)hash=fnv_byte(hash,value>>(i*8));return hash;
}
void fm_probe_ledger(unsigned *row){
 unsigned h=2166136261u;
 for(unsigned i=0;i<0x574;i++)h=fnv_byte(h,raw8(0xff7a00+i));row[0]=h;
 h=2166136261u;
 for(unsigned i=0;i<count;i++){
  h=fnv_word(h,events[i].frame);h=fnv_word(h,events[i].master);
  h=fnv_byte(h,events[i].port);h=fnv_byte(h,events[i].data);
 }
 row[1]=h;row[2]=count;row[3]=(unsigned)m68k.cycles;
}

#include "forecourt_driver_state_r22.inc"

void fm_driver_end(void){
 free(memory);free(events);free(drums);memory=NULL;events=NULL;drums=NULL;
 count=capacity=drum_count=drum_capacity=0;failure=bad_address=0;
}
