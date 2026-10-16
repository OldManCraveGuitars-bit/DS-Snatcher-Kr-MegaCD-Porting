/* Source-computed live-driver/YM boundary state; no native pointers on wire. */
#include "forecourt_source_identity.h"
extern unsigned fm_driver_export(void *,unsigned),fm_chip_export(void *,unsigned);
extern unsigned fm_chip_export_portable(void *,unsigned);
extern int fm_driver_import(const void *,unsigned,int),fm_chip_import(const void *,unsigned);
extern int fm_driver_prepare_state(const void *,unsigned,unsigned *);
static unsigned state_get32(const uint8_t *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static void state_put32(uint8_t *p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
#include "resource_checksum.h"
static unsigned state_crc(const uint8_t *p,unsigned size){return resource_crc32(p,size);}
static unsigned forecourt_fm_export_mode(void *data,unsigned size,bool portable){
 if(!fm_ready || !data || size<128 || request_count>32)return 0;
 uint8_t *p=data;memset(p,0,128);
 unsigned driver=fm_driver_export(p+128,size-128);if(!driver)return 0;
 unsigned chip=portable?fm_chip_export_portable(p+128+driver,size-128-driver):fm_chip_export(p+128+driver,size-128-driver);if(!chip)return 0;
 const unsigned fields[]={ (unsigned)clock,(unsigned)(clock>>32),phase,position,event_at,
    driver_frame,driver_calls,driver_events,fm_sample_hash,(unsigned)last[0],(unsigned)last[1],
    (unsigned)previous[0],(unsigned)previous[1],(unsigned)filtered[0],(unsigned)filtered[1],driver,chip};
 unsigned tail=request_count*8,base=128+driver+chip;
 if(tail>size-base)return 0;
 unsigned version=sub_music_bridge?3:(automatic_score==0x4c && !request_count?1:2);
 memcpy(p,"NSFL",4);state_put32(p+4,version);state_put32(p+8,base+tail);
 memcpy(p+16,forecourt_source_identity,32);
 for(unsigned i=0;i<17;i++)state_put32(p+48+i*4,fields[i]);
 if(version>=2){
  state_put32(p+116,automatic_score);state_put32(p+120,request_count);
  for(unsigned i=0;i<request_count;i++){
   unsigned at=(request_read+i)&31;
   state_put32(p+base+i*8,requests[at].frame);state_put32(p+base+i*8+4,requests[at].command);
  }
 }
 if(version==3){p[124]=sub_music_level;p[125]=sub_music_fading;p[127]=1;}
 state_put32(p+12,state_crc(p+16,base+tail-16));return base+tail;
}
unsigned forecourt_fm_export_state(void *data,unsigned size){return forecourt_fm_export_mode(data,size,false);}
unsigned forecourt_fm_export_portable_state(void *data,unsigned size){return forecourt_fm_export_mode(data,size,true);}
bool forecourt_fm_import_state(const void *data,unsigned size){
 if(!fm_ready || !data || size<128)return false;
 const uint8_t *p=data;
 unsigned version=state_get32(p+4);
 if(memcmp(p,"NSFL",4) || (version!=1 && version!=2 && version!=3) || state_get32(p+8)!=size ||
    state_get32(p+12)!=state_crc(p+16,size-16) || memcmp(p+16,forecourt_source_identity,32))return false;
 unsigned v[17];for(unsigned i=0;i<17;i++)v[i]=state_get32(p+48+i*4);
 uint64_t new_clock=v[0]|(uint64_t)v[1]<<32;
 unsigned automatic=version==1?0x4c:state_get32(p+116),count=version==1?0:state_get32(p+120);
 if(automatic>255 || count>32 || (version==3?(p[125]>1 || p[126] || p[127]!=1):state_get32(p+124)) ||
    (version==1 && (state_get32(p+116) || state_get32(p+120))))return false;
 if(v[2]>=1008u*32000 || v[4]>4096 || v[15]>size-128 || v[16]>size-128-v[15] ||
    count*8!=size-128-v[15]-v[16] ||
    new_clock/896040>v[5]+1u || new_clock/896040+1u<v[5])return false;
 unsigned base=128+v[15]+v[16],previous_frame=v[5];
 for(unsigned i=0;i<count;i++){
  unsigned frame=state_get32(p+base+i*8),command=state_get32(p+base+i*8+4);
  if(frame<=v[5] || frame<previous_frame || command>(version==3?0x2ffu:255u))return false;
  previous_frame=frame;
 }
 /* Validate the event cursor and reserve driver storage before applying the
  * transactional chip state. A rejected save must not change live audio. */
 unsigned event_total;
 if(!fm_driver_prepare_state(p+128,v[15],&event_total) || v[4]>event_total)return false;
 if(!fm_chip_import(p+128+v[15],v[16]))return false;
 if(!fm_driver_import(p+128,v[15],1)){fm_ready=0;return false;}
 if(v[4]>fm_probe_info(0)){fm_ready=0;return false;}
 clock=new_clock;phase=v[2];position=v[3];event_at=v[4];driver_frame=v[5];
 driver_calls=v[6];driver_events=v[7];fm_sample_hash=v[8];last[0]=v[9];last[1]=v[10];
 previous[0]=v[11];previous[1]=v[12];filtered[0]=v[13];filtered[1]=v[14];
 automatic_score=automatic;request_read=0;request_count=count;
 sub_music_bridge=version==3;sub_music_level=version==3?p[124]:255;sub_music_fading=version==3?p[125]:0;
 for(unsigned i=0;i<count;i++){
  requests[i].frame=state_get32(p+base+i*8);requests[i].command=state_get32(p+base+i*8+4);
 }
 driver_bus_cycles=0;fm_error=0;return true;
}
