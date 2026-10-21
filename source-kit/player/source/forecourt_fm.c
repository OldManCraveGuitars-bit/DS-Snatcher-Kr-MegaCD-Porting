#ifdef __NDS__
#include "fm_demand_table.h"
#endif
#include "system_perf.h"
#include "settings.h"
/* Private original-driver + continuous six-channel YM2612 probe.

 * No PCM loop or precomputed register timeline is used by this player.

 * The independent full-core timeline is used only by its verifier.

 */

#include "ym2612.h"
#include "resource_audio_clock.h"

#include "resource_live_sound.h"

#include <stdint.h>

#include <stdbool.h>

#include <string.h>

#include <stdlib.h>

#ifdef __NDS__

#include <nds.h>

#else

#define ITCM_CODE

#endif

typedef struct {uint32_t frame,master;uint8_t port,data;} source_event;

extern int fm_probe_init(const void *,unsigned,const void *,unsigned,int);

extern int fm_probe_execute(unsigned);

extern void fm_probe_frame(unsigned),fm_probe_reg(unsigned,unsigned);

extern void fm_probe_write(unsigned,const void *,unsigned),fm_probe_clear_events(void);

extern void fm_probe_read(unsigned,void *,unsigned);

extern unsigned fm_probe_info(unsigned),fm_probe_bad_address(void),fm_compact_fault;

extern const source_event *fm_probe_events(void);

extern bool fm_tables_begin(void);



extern void fm_observe_stack(void);

extern volatile unsigned fm_error,fm_ready,fm_sample_hash;

volatile unsigned driver_frame,driver_calls,driver_events;

volatile uint64_t driver_bus_cycles;

static uint64_t clock;

static unsigned phase,position,event_at;

static int last[2];

static int32_t previous[2],filtered[2];

static unsigned automatic_score;
#include "../generated/original_sound_gain.h"
static uint8_t sub_music_level=255,sub_music_fading;
static bool sub_music_bridge;
unsigned forecourt_fm_music_level(void){return sub_music_level;}
static void sub_music_gain_write(void){
 uint8_t level=sub_music_gain[sub_music_level];fm_probe_write(0xff7f5e,&level,1);
}


static struct {unsigned frame;uint16_t command;} requests[32];

static unsigned request_read,request_count;

static resource_live_sound_observer observer;

static void *observer_context;



bool forecourt_fm_schedule(unsigned frame,unsigned command){

 if(!fm_ready || command>255 || frame<=driver_frame || request_count==32)return false;

 unsigned at=(request_read+request_count)&31;

 if(request_count && frame<requests[(at-1)&31].frame)return false;

 requests[at].frame=frame;requests[at].command=command;request_count++;return true;

}

bool forecourt_fm_request_sub(unsigned frame,unsigned command,unsigned channel){
 if(channel>1 || !forecourt_fm_schedule(frame,command))return false;
 unsigned at=(request_read+request_count-1)&31;
 requests[at].command|=0x100u+(channel<<8);sub_music_bridge=true;return true;
}
void forecourt_fm_observe(resource_live_sound_observer callback,void *context){

 observer=callback;observer_context=context;

}



extern unsigned fm_probe_discard_events(unsigned);
static bool next_frame(void){

#ifdef __NDS__

 unsigned begin=cpuGetTiming();

#endif

 unsigned retained=fm_probe_discard_events(event_at);driver_frame++;fm_probe_frame(driver_frame);event_at=0;

 if(driver_frame==1 && automatic_score){

  fm_probe_reg(0,0x3f);

  if(!fm_probe_execute(0xff5058) || !fm_probe_execute(0xff5106))return false;

  fm_probe_frame(1);

 }

 if(driver_frame==961 && automatic_score){fm_probe_reg(0,automatic_score);if(!fm_probe_execute(0xff5058))return false;}

 while(request_count && requests[request_read].frame==driver_frame){

  unsigned code=requests[request_read].command,command=code&255;
  if(code>=0x100 && command==255){
   sub_music_fading=1; /* Original15EB8: set7F815; do not enqueue FF. */
  }else{
   if(code>=0x200 && command && command<0xf1){sub_music_fading=0;sub_music_level=255;}
   fm_probe_reg(0,command);
   if(!fm_probe_execute(0xff5058))return false;
  }

  request_read=(request_read+1)&31;request_count--;

 }

 /* Original15242..1527E: subtract3 once per SUB service until below3.
  * MAIN receives the logarithmic15FBA gain; source PCM drums use linear. */
 if(sub_music_bridge){
  if(sub_music_fading && sub_music_level>=3)sub_music_level-=3;
  sub_music_gain_write();
 }
 if(!fm_probe_execute(0xff5106) || !fm_probe_execute(0xff408a) ||

    fm_compact_fault || fm_probe_bad_address())return false;

 driver_calls++;driver_events+=fm_probe_info(0)-retained;

 if(observer){

  uint8_t communication[5];fm_probe_read(0xa1201a,communication,5);

  unsigned pcm=(unsigned)communication[0]<<24|(unsigned)communication[1]<<16|

      (unsigned)communication[2]<<8|communication[3];

  observer(observer_context,driver_frame,communication[4],pcm);

 }

#ifdef __NDS__

 driver_bus_cycles+=cpuGetTiming()-begin;

#endif

 return true;

}



bool forecourt_fm_begin_native(const void *ip,unsigned ip_size,const void *fm,unsigned fm_size){

 fm_error=fm_ready=phase=position=event_at=driver_frame=driver_calls=driver_events=0;

 driver_bus_cycles=clock=0;fm_sample_hash=2166136261u;

 automatic_score=0x4c;request_read=request_count=0;observer=NULL;observer_context=NULL;
 sub_music_bridge=false;sub_music_fading=0;sub_music_level=255;

 memset(last,0,sizeof(last));memset(previous,0,sizeof(previous));memset(filtered,0,sizeof(filtered));

 if(!fm_probe_init(ip,ip_size,fm,fm_size,1) || !fm_probe_execute(0xff56ae))return false;

 fm_probe_frame(0);if(!fm_probe_execute(0xff52ac))return false;

 const unsigned char gates[2]={255,255};fm_probe_write(0xff7f5e,gates,2);

 if(!fm_tables_begin())return false;

 YM2612Init_(144000,1000,ST_SSG|ST_DAC);

#ifdef __NDS__

 extern bool fm_parallel_init(void);if(!fm_parallel_init())return false;

#endif

 fm_ready=1;return true;

}

bool forecourt_fm_begin_shared(const void *ip,unsigned ip_size,const void *fm,unsigned fm_size){

 if(!forecourt_fm_begin_native(ip,ip_size,fm,fm_size))return false;

 automatic_score=0;return true;

}



static bool forecourt_fm_render_body(int16_t *out,unsigned n){

 fm_observe_stack();

 while(n){

  /* 128 output frames expose longer ARM7 work while remaining below
   * one source tick. Timed YM writes still split the native sample span. */
  unsigned block=n>128?128:n;

  unsigned needed=resource_fm_native_count(phase,block);

  s32 raw[256];unsigned made=0;
#ifdef __NDS__
  /* Match the unchanged output converter exactly, including a fractional
   * starting phase and blocks split by an original timed register write. */
  /* Lower-rate FM consumes every internal sample. */
#endif

  if(needed>128)return false;

  while(made<needed){

   /* Source VBlank boundaries without a register write must not subdivide

    * the established YM native block. Look ahead only as far as this output

    * block requires; queue original writes without applying future writes. */

   uint64_t limit=clock+(needed-made)*4032u;

   unsigned count;const source_event *events;

   for(;;){

    count=fm_probe_info(0);events=fm_probe_events();

    while(event_at<count){

     const source_event *e=events+event_at;

     uint64_t time=(uint64_t)e->frame*896040+e->master;

     if(time>clock)break;

     if(e->port<4)YM2612Write_(e->port,e->data);

     else if(e->port!=4 || (e->data!=0x9f && e->data!=0xbf && e->data!=0xdf && e->data!=0xff))return false;

     event_at++;

    }

    if(event_at<count || ((uint64_t)driver_frame+1)*896040>=limit)break;

    if(!next_frame())return false;

   }

   uint64_t next=limit;

   if(event_at<count){const source_event *e=events+event_at;

    uint64_t time=(uint64_t)e->frame*896040+e->master;if(time<next)next=time;}

   unsigned distance=resource_fm_native_distance(next-clock);

   unsigned take=distance>needed-made?needed-made:(unsigned)distance;

   if(!take)return false;

#ifdef __NDS__
   extern void YM2612SetOutputDemand_(const unsigned char *);
   YM2612SetOutputDemand_(0);
#endif
   unsigned mask=63;
   if(!settings_bgm || !settings_effects){
       uint8_t effects;fm_probe_read(0xff7f24,&effects,1);mask=settings_fm_mask(effects);
   }
   YM2612SetChannelMask_(mask);
   YM2612UpdateOne_(raw+made*2,take,1,1);
#ifdef __NDS__
   YM2612SetOutputDemand_(0);
#endif
   made+=take;clock+=take*4032;

  }

#ifdef __NDS__

  extern unsigned fm_output_filter_arm_unhashed(uint32_t *,const s32 *,int16_t *,unsigned);

  uint32_t state[8]={phase,(uint32_t)last[0],(uint32_t)last[1],(uint32_t)previous[0],

      (uint32_t)previous[1],(uint32_t)filtered[0],(uint32_t)filtered[1],fm_sample_hash};

  unsigned consumed=fm_output_filter_arm_unhashed(state,raw,out,block);

  phase=state[0];last[0]=(int32_t)state[1];last[1]=(int32_t)state[2];

  previous[0]=(int32_t)state[3];previous[1]=(int32_t)state[4];

  filtered[0]=(int32_t)state[5];filtered[1]=(int32_t)state[6];fm_sample_hash=state[7];

  /* The shared final-output hash still checks every emitted stereo sample.
   * fm_sample_hash is diagnostic state only, retained for save compatibility. */
  position+=block;

#else

  unsigned consumed=0;

  for(unsigned i=0;i<block;i++,position++){

   phase+=53693175;

   while(phase>=4032u*32000){phase-=4032u*32000;last[0]=raw[consumed*2];last[1]=raw[consumed*2+1];consumed++;}

   for(unsigned ch=0;ch<2;ch++){

    int32_t v=last[ch]*2;

    filtered[ch]=v-previous[ch]+(int32_t)(((int64_t)filtered[ch]*65279)/65536);

    previous[ch]=v;int32_t value=filtered[ch];

    out[i*2+ch]=value>32767?32767:value< -32768?-32768:value;

    fm_sample_hash=(fm_sample_hash^(uint16_t)out[i*2+ch])*16777619u;

   }

  }

#endif

  if(consumed!=needed)return false;
  while(resource_audio_frame_boundary(driver_frame+1)<position){
   if(!next_frame())return false;
  }

  out+=block*2;n-=block;

 }

 return true;

}



#include "forecourt_live_state_r22.inc"


bool forecourt_fm_render(int16_t *out,unsigned n){
    unsigned previous=system_perf_enter(PERF_FM);
    bool result=forecourt_fm_render_body(out,n);
    system_perf_leave(previous);return result;
}
