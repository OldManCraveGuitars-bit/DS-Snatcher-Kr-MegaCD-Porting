#include "resource_room_audio.h"
#include <string.h>
static uint32_t read32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
void resource_room_audio_drums_end(resource_room_audio *s){if(s)s->drum_commands=NULL;}
bool resource_room_audio_drums_begin(resource_room_audio *s,const void *data,size_t bytes){
    const uint8_t *p=data;
    if(!s || !p || bytes<32 || memcmp(p,"NSDM",4))return false;
    unsigned version=read32(p+4);
    unsigned count=read32(p+8),start=read32(p+12),end=read32(p+16),delta=read32(p+20);
    if(!count || count>240000 || start>=count || end!=count || delta>255)return false;
    if(version==1){
        if(read32(p+24) || read32(p+28) || bytes!=32+(size_t)count*4)return false;
    }else if(version==2){
        unsigned records=read32(p+24);
        if(!records || records>count || read32(p+28)!=8 || bytes!=32+(size_t)records*8 || read32(p+32))return false;
        unsigned previous=0;
        for(unsigned i=0;i<records;i++){
            unsigned tick=read32(p+32+i*8);
            if(tick>=count || (i && tick<=previous))return false;
            previous=tick;
        }
    }else return false;
    if(!resource_pcm_driver_drum_bank(&s->driver))return false;
    s->drum_commands=p+32;s->drum_count=count;s->drum_loop_start=start;s->drum_sequence_delta=delta;
    s->drum_samples=s->drum_ticks=s->next_drum_tick=s->drum_loops=0;return true;
}
static const uint8_t *drum_row(const uint8_t *rows,unsigned frame){
    /* Densev1 and lossless change-onlyv2 share the same source frame clock.
     * Binary search also works at loop rewind; no mutable cursor to restore. */
    if(read32(rows-28)==1)return rows+frame*4;
    unsigned lo=0,hi=read32(rows-8);
    while(lo+1<hi){unsigned mid=lo+(hi-lo)/2;if(read32(rows+mid*8)<=frame)lo=mid;else hi=mid;}
    return rows+lo*8+4;
}
bool resource_room_audio_bind(resource_room_audio *s,const resource_pcm_effect_bank *bank){
    if(!s)return false;
    memset(s,0,sizeof(*s));
    return resource_pcm_driver_bind(&s->driver,bank) && resource_pcm_output_reset(&s->output,32000);
}
bool resource_room_audio_render(resource_room_audio *s,int16_t *out,unsigned frames){
    if(!s || !out)return false;
    while(frames){
        if(s->drum_commands && s->drum_samples==s->next_drum_tick){
            unsigned row=s->drum_ticks,loops=0;
            if(row>=s->drum_count){unsigned period=s->drum_count-s->drum_loop_start;
                loops=(row-s->drum_loop_start)/period;row=s->drum_loop_start+(row-s->drum_loop_start)%period;}
            const uint8_t *p=drum_row(s->drum_commands,row);
            if(!resource_pcm_driver_drums(&s->driver,p[0],p[1],p[2],(p[3]+loops*s->drum_sequence_delta)&255))return false;
            s->drum_loops=loops;s->next_drum_tick=((uint64_t)++s->drum_ticks*32000*896040+53693174)/53693175;
        }
        if(s->samples==s->next_tick){
            if(!resource_pcm_driver_tick(&s->driver))return false;
            s->next_tick=((uint64_t)++s->ticks*32000*896040+53693174)/53693175;
        }
        unsigned n=s->next_tick-s->samples;if(n>frames)n=frames;
        if(s->drum_commands && n>s->next_drum_tick-s->drum_samples)n=s->next_drum_tick-s->drum_samples;
        resource_pcm_output_render(&s->output,&s->driver.chip,out,n);
        s->samples+=n;if(s->drum_commands)s->drum_samples+=n;out+=n*2;frames-=n;
    }
    return true;
}
