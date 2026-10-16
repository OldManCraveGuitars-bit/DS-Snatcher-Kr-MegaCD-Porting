#include "ym2612.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __NDS__
#include <nds.h>
#else
#define ITCM_CODE
#endif
void memset32(void *dest,int value,int count){
 int *p=dest;while(count--)*p++=value;
}

#ifdef __NDS__
extern char __sbss_end;
volatile unsigned fm_stack_low=0xffffffffu,fm_stack_data_end,fm_stack_bad;
void fm_observe_stack(void){
 unsigned pointer;__asm__ volatile("mov %0,sp":"=r"(pointer));
 fm_stack_data_end=(unsigned)&__sbss_end;
 if(pointer<fm_stack_low)fm_stack_low=pointer;
 if(pointer<fm_stack_data_end+64)fm_stack_bad=1;
}
#else
void fm_observe_stack(void){}
#endif
static unsigned char fm_start_event_sha256[32];
static bool skip_preroll;
volatile unsigned fm_preroll_samples;
bool probe_begin_fast(const char *,const char *);
static FILE *event_file;
static uint8_t event_cache[8192] __attribute__((aligned(32)));
static unsigned cache_at,cache_count;
static unsigned count,loop_at,at,phase,position;
static uint64_t loop_start,loop_end,clock;
volatile unsigned fm_loops,fm_stream_reads,fm_stream_seeks,fm_ready;
volatile unsigned fm_sample_hash=2166136261u;
static int last[2];
static int32_t previous[2],filtered[2];
static unsigned read32(const uint8_t *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static uint64_t read64(const uint8_t *p){return read32(p)|((uint64_t)read32(p+4)<<32);}
static const uint8_t *read_event(unsigned index){
 fm_observe_stack();
 if(index>=count)return NULL;
 if(index<cache_at || index-cache_at>=cache_count){
  unsigned take=count-index;if(take>512)take=512;
  if(fseek(event_file,64+(long)index*16,SEEK_SET))return NULL;
  fm_stream_seeks++;
  if(fread(event_cache,16,take,event_file)!=take)return NULL;
  fm_stream_reads++;cache_at=index;cache_count=take;
 }
 return event_cache+(index-cache_at)*16;
}
bool probe_render(int16_t *,unsigned);
bool probe_begin_stream(const char *path){
 fm_ready=0;
 if(event_file){fclose(event_file);event_file=NULL;}
 event_file=fopen(path,"rb");if(!event_file)return false;
 uint8_t header[64];if(fread(header,1,64,event_file)!=64)return false;
 if(memcmp(header,"NSFS",4) || read32(header+4)!=1 || read32(header+8)!=16 ||
    read32(header+24)!=32000 || read32(header+28)!=64)return false;
 count=read32(header+12);loop_at=read32(header+16);
 loop_start=read64(header+32);loop_end=read64(header+40);
 uint64_t preroll=read64(header+48),bytes=read64(header+56);
 if(!count || count>0x07ffffff || loop_at>=count || loop_start>=loop_end ||
    bytes!=64+(uint64_t)count*16 || preroll>0xffffffffu)return false;
 if(fseek(event_file,0,SEEK_END) || (uint64_t)ftell(event_file)!=bytes)return false;
 at=phase=position=fm_loops=cache_at=cache_count=fm_stream_reads=fm_stream_seeks=fm_ready=0;clock=0;
 fm_sample_hash=2166136261u;
 memset(last,0,sizeof(last));memset(previous,0,sizeof(previous));memset(filtered,0,sizeof(filtered));
 YM2612Init_(144000,1000,ST_SSG|ST_DAC);
#ifdef __NDS__
 extern bool fm_parallel_init(void);if(!fm_parallel_init())return false;
#endif
 /* Diagnostic only: execute every earlier source event and native tick before
  * measuring the requested score. No saved chip state or captured audio. */
 static int16_t scratch[256];fm_preroll_samples=0;
 if(skip_preroll)preroll=0;
 while(preroll){unsigned n=preroll>128?128:(unsigned)preroll;
  if(!probe_render(scratch,n))return false;fm_preroll_samples+=n;preroll-=n;}
 fm_sample_hash=2166136261u;fm_ready=1;return true;
}
bool probe_render(int16_t *out,unsigned n){
 fm_observe_stack();
 while(n){
  unsigned block=n>64?64:n;
  unsigned needed=(phase+(uint64_t)block*53693175)/(1008u*32000);
  s32 raw[256];unsigned made=0;
  if(needed>128)return false;
  while(made<needed){
   if(clock>=loop_end){clock-=loop_end-loop_start;at=loop_at;fm_loops++;}
   while(at<count){
    const uint8_t *p=read_event(at);if(!p)return false;
    if(read64(p)>clock)break;
    if(p[8]<4)YM2612Write_(p[8],p[9]);at++;
   }
   uint64_t next=loop_end;
   if(at<count){const uint8_t *p=read_event(at);if(!p)return false;
    uint64_t event_clock=read64(p);if(event_clock<next)next=event_clock;}
   uint64_t until=(next-clock+1007)/1008;
   unsigned take=until>needed-made?needed-made:(unsigned)until;
   if(!take)return false;
   YM2612UpdateOne_(raw+made*2,take,1,1);made+=take;clock+=take*1008;
  }
#ifdef __NDS__
  extern unsigned fm_output_filter_arm(uint32_t *,const s32 *,int16_t *,unsigned);
  uint32_t state[8]={phase,(uint32_t)last[0],(uint32_t)last[1],(uint32_t)previous[0],
      (uint32_t)previous[1],(uint32_t)filtered[0],(uint32_t)filtered[1],fm_sample_hash};
  unsigned consumed=fm_output_filter_arm(state,raw,out,block);
  phase=state[0];last[0]=(int32_t)state[1];last[1]=(int32_t)state[2];
  previous[0]=(int32_t)state[3];previous[1]=(int32_t)state[4];
  filtered[0]=(int32_t)state[5];filtered[1]=(int32_t)state[6];fm_sample_hash=state[7];
  position+=block;
#else
  unsigned consumed=0;
  for(unsigned i=0;i<block;i++,position++){
   phase+=53693175;
   while(phase>=1008u*32000){
    phase-=1008u*32000;
    last[0]=raw[consumed*2];last[1]=raw[consumed*2+1];consumed++;
   }
   for(unsigned ch=0;ch<2;ch++){
    int32_t v=last[ch]*2;
    filtered[ch]=v-previous[ch]+(int32_t)(((int64_t)filtered[ch]*65279)/65536);
    previous[ch]=v;int32_t sample=filtered[ch];
    out[i*2+ch]=sample>32767?32767:sample< -32768?-32768:sample;
    fm_sample_hash=(fm_sample_hash^(uint16_t)out[i*2+ch])*16777619u;
   }
  }
#endif
  if(consumed!=needed)return false;
  out+=block*2;n-=block;
 }
 return true;
}
#include "fm_stream_checkpoint_r22.inc"


extern bool forecourt_fm_begin_native(const void *,unsigned,const void *,unsigned);
extern bool forecourt_fm_render(int16_t *,unsigned),forecourt_fm_import_state(const void *,unsigned);
extern void fm_driver_end(void);
static bool live_forecourt;
static void *read_forecourt_data(const char *path,unsigned *bytes){
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 bool good=fseek(f,0,SEEK_END)==0;long n=good?ftell(f):-1;
 good=good && n>0 && n<=1048576 && fseek(f,0,SEEK_SET)==0;
 void *p=good?malloc(n):NULL;
 if(p && fread(p,1,n,f)!=(unsigned)n){free(p);p=NULL;}
 fclose(f);if(p)*bytes=(unsigned)n;return p;
}
static bool begin_forecourt(void){
 unsigned ni=0,nf=0,ns=0,begin=cpuGetTiming();
 void *ip=read_forecourt_data("nitro:/native-driver-4c-ip.bin",&ni);
 void *fm=read_forecourt_data("nitro:/native-driver-4c-fm.bin",&nf);
 bool good=ip && fm && forecourt_fm_begin_native(ip,ni,fm,nf);
 free(ip);free(fm);
 void *state=good?read_forecourt_data("nitro:/native-driver-4c-start.bin",&ns):NULL;
 good=state && forecourt_fm_import_state(state,ns);free(state);
 if(good){fm_fast_init=1;fm_start_state_bytes=ns;fm_preroll_samples=0;
     fm_sample_hash=2166136261u;fm_begin_cycles=cpuGetTiming()-begin;}
 return good;
}

volatile unsigned fm_error,fm_command,fm_epoch,fm_fill_count,fm_written;
volatile unsigned fm_fill_sizes[4096],fm_fill_hashes[4096];
extern bool fm_tables_begin(void);
extern void fm_tables_end(void);
void resource_fm_close(void){
 fm_driver_end();live_forecourt=false; fm_tables_end();fm_ready=0;fm_command=0;if(event_file){fclose(event_file);event_file=NULL;}
}
bool resource_fm_active(void){return fm_ready && fm_command;}
bool resource_fm_open(unsigned command){
 resource_fm_close();fm_error=0;fm_fill_count=fm_written=0;fm_epoch++;
 if(command==0x4c){
  if(!begin_forecourt()){resource_fm_close();fm_error=11;return false;}
  live_forecourt=true;fm_command=command;return true;
 }
 const char *events,*state;
 if(command==64){
  const unsigned char identity[32]={84,82,229,172,206,109,208,20,201,47,207,169,115,244,65,104,119,205,80,244,189,5,76,127,199,1,91,16,74,39,20,185};
  memcpy(fm_start_event_sha256,identity,32);events="nitro:/native-fm-40.nsfs";state="nitro:/native-fm-40.nsfi";
 }
 else if(command==0x5e){
  const unsigned char identity[32]={75,15,129,45,201,113,2,132,248,33,15,19,154,70,220,11,72,15,237,123,116,22,95,241,149,106,27,90,144,188,85,31};
  memcpy(fm_start_event_sha256,identity,32);events="nitro:/native-fm-5e.nsfs";state="nitro:/native-fm-5e.nsfi";
 }
 else if(command==95){
  const unsigned char identity[32]={187,135,174,17,216,217,124,126,253,252,236,31,23,107,108,104,171,88,245,183,65,217,48,20,57,67,180,245,79,19,57,9};
  memcpy(fm_start_event_sha256,identity,32);events="nitro:/native-fm-5f.nsfs";state="nitro:/native-fm-5f.nsfi";
 }
 else if(command==0x62){
  const unsigned char identity[32]={47,116,224,76,243,220,136,122,127,250,176,56,225,146,30,122,125,1,158,128,120,226,61,56,41,17,139,217,31,55,18,79};
  memcpy(fm_start_event_sha256,identity,32);events="nitro:/native-fm-62.nsfs";state="nitro:/native-fm-62.nsfi";
 }
 else return false;
 if(!fm_tables_begin() || !probe_begin_fast(events,state)){resource_fm_close();fm_error=10;return false;}
 fm_command=command;return true;
}
bool resource_fm_render(int16_t *out,unsigned frames){
 if(!resource_fm_active() || !(live_forecourt?forecourt_fm_render(out,frames):probe_render(out,frames)) || fm_error)return false;
 {unsigned slot=fm_fill_count&4095u;fm_fill_sizes[slot]=frames;fm_fill_hashes[slot]=fm_sample_hash;}
 fm_fill_count++;fm_written+=frames;return true;
}
