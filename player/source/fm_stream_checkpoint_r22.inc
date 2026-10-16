/* Portable source-compiled FM stream start state. Include in the stream unit. */
#include "fm_start_identity.h"
extern unsigned fm_chip_export(void *,unsigned);
extern int fm_chip_import(const void *,unsigned);
static void start_put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
static uint32_t start_crc(const uint8_t *p,unsigned bytes){
    uint32_t crc=~0u;
    for(unsigned i=0;i<bytes;i++){crc^=p[i];for(unsigned b=0;b<8;b++)crc=(crc>>1)^(0xedb88320u&-(crc&1));}
    return ~crc;
}
unsigned probe_export_state(void *destination,unsigned capacity){
    if(!fm_ready || !destination || capacity<128)return 0;
    uint8_t *p=destination;
    const uint32_t fields[19]={count,loop_at,(uint32_t)loop_start,(uint32_t)(loop_start>>32),
        (uint32_t)loop_end,(uint32_t)(loop_end>>32),at,phase,position,(uint32_t)clock,(uint32_t)(clock>>32),
        fm_loops,fm_sample_hash,(uint32_t)last[0],(uint32_t)last[1],(uint32_t)previous[0],(uint32_t)previous[1],
        (uint32_t)filtered[0],(uint32_t)filtered[1]};
    memcpy(p+16,fm_start_event_sha256,32);
    for(unsigned i=0;i<19;i++)start_put32(p+48+4*i,fields[i]);
    unsigned chip=fm_chip_export(p+128,capacity-128);if(!chip)return 0;
    start_put32(p+124,chip);memcpy(p,"NSFI",4);start_put32(p+4,1);start_put32(p+8,128+chip);
    start_put32(p+12,start_crc(p+16,112+chip));return 128+chip;
}
bool probe_import_state(const void *source,unsigned bytes){
    if(!source || !event_file || bytes<128)return false;
    const uint8_t *p=source;
    if(memcmp(p,"NSFI",4) || read32(p+4)!=1 || read32(p+8)!=bytes || read32(p+12)!=start_crc(p+16,bytes-16) ||
       memcmp(p+16,fm_start_event_sha256,32) || read32(p+124)!=bytes-128)return false;
    uint32_t value[19];for(unsigned i=0;i<19;i++)value[i]=read32(p+48+i*4);
    if(value[0]!=count || value[1]!=loop_at || ((uint64_t)value[3]<<32|value[2])!=loop_start ||
       ((uint64_t)value[5]<<32|value[4])!=loop_end || value[6]>count || value[7]>=1008u*32000 ||
       ((uint64_t)value[10]<<32|value[9])>=loop_end+1008)return false;
    if(!fm_chip_import(p+128,bytes-128))return false;
    at=value[6];phase=value[7];position=value[8];clock=(uint64_t)value[10]<<32|value[9];
    fm_loops=value[11];fm_sample_hash=value[12];last[0]=value[13];last[1]=value[14];
    previous[0]=value[15];previous[1]=value[16];filtered[0]=value[17];filtered[1]=value[18];
    cache_at=cache_count=0;fm_stream_reads=fm_stream_seeks=0;fm_ready=1;return true;
}
volatile unsigned fm_fast_init,fm_start_state_bytes,fm_begin_cycles;
bool probe_begin_fast(const char *events_path,const char *state_path){
    static uint8_t source_state[8192];
#ifdef __NDS__
    unsigned begin=cpuGetTiming();
#endif
    fm_fast_init=fm_start_state_bytes=0;skip_preroll=true;
    bool good=probe_begin_stream(events_path);skip_preroll=false;fm_ready=0;
    if(!good)return false;
    FILE *f=fopen(state_path,"rb");if(!f)return false;
    good=fseek(f,0,SEEK_END)==0;long bytes=good?ftell(f):-1;
    good=good && bytes>=128 && bytes<=(long)sizeof(source_state) && fseek(f,0,SEEK_SET)==0;
    if(good)good=fread(source_state,1,bytes,f)==(size_t)bytes;
    fclose(f);if(!good || !probe_import_state(source_state,(unsigned)bytes))return false;
    fm_fast_init=1;fm_start_state_bytes=(unsigned)bytes;
#ifdef __NDS__
    fm_begin_cycles=cpuGetTiming()-begin;
#endif
    return true;
}
