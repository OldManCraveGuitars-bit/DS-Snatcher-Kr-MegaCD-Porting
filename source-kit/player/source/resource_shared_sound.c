#include "settings.h"
#include "resource_shared_sound.h"
#include "resource_live_sound.h"
#include <string.h>
#include "resource_audio_clock.h"

extern bool forecourt_fm_render(int16_t *,unsigned);
static resource_shared_sound *shared_owner;
uint64_t resource_shared_sound_boundary(unsigned frame){
    return resource_audio_frame_boundary(frame);
}
static void observe(void *context,unsigned frame,uint8_t signal,uint32_t pcm){
    resource_shared_sound *s=context;
    /* A full queue is a visible failure, never permission to discard a
     * source tick, channel request or completion while the DAC is behind. */
    if(s->error)return;
    if(frame!=s->generated_frame+1 || s->notice_count==RESOURCE_SHARED_SOUND_NOTICES || s->pcm_count==4){s->error=1;return;}
    resource_sound_notification n={frame,pcm,signal,(uint8_t)forecourt_fm_music_level()};
    s->notices[(s->notice_read+s->notice_count++)&RESOURCE_SHARED_SOUND_NOTICE_MASK]=n;
    s->pending_pcm[(s->pcm_read+s->pcm_count++)&3]=n;
    s->generated_frame=frame;
}
bool resource_shared_sound_begin(resource_shared_sound *s,const resource_pcm_effect_bank *bank,
    const void *ip,unsigned ip_bytes,const void *fm,unsigned fm_bytes){
    if(!s)return false;
    shared_owner=NULL;
    forecourt_fm_observe(NULL,NULL);
    memset(s,0,sizeof(*s));
    if(!resource_room_audio_bind(&s->pcm,bank) || !resource_pcm_driver_drum_bank(&s->pcm.driver) ||
       !forecourt_fm_begin_shared(ip,ip_bytes,fm,fm_bytes))return false;
    forecourt_fm_observe(observe,s);s->initialized=true;shared_owner=s;return true;
}
void resource_shared_sound_end(resource_shared_sound *s){
    if(s && shared_owner==s){forecourt_fm_observe(NULL,NULL);shared_owner=NULL;s->initialized=false;}
}
bool resource_shared_sound_request(resource_shared_sound *s,unsigned frame,unsigned command,unsigned channel){
    if(!s || shared_owner!=s || !s->initialized || s->error || channel>1 || command>255)return false;
    if(!command)return true; /* Original zero request does not replace a cue. */
    if(!forecourt_fm_request_sub(frame,command,channel))return false;
    if(channel){s->music=command==255?0:command;s->music_request=frame;}
    else {s->effect=command==0x3d?0:command;s->effect_request=frame;}
    return true;
}
static bool tick_pcm(resource_shared_sound *s){
    if(s->next_frame){
        if(!s->pcm_count || s->pending_pcm[s->pcm_read].frame!=s->next_frame){s->error=2;return false;}
        uint32_t command=s->pending_pcm[s->pcm_read].pcm;
        resource_pcm_driver_music_level(&s->pcm.driver,s->pending_pcm[s->pcm_read].music_level);
        s->pcm_read=(s->pcm_read+1)&3;s->pcm_count--;
        if(!resource_pcm_driver_drums(&s->pcm.driver,command>>24,(command>>16)&255,
                (command>>8)&255,command&255)){s->error=3;return false;}
    }
    if(!resource_pcm_driver_tick(&s->pcm.driver)){s->error=4;return false;}
    s->next_frame++;return true;
}
static bool render_output(resource_shared_sound *s,int16_t *out,unsigned samples,uint32_t *hash){
    if(!s || shared_owner!=s || !s->initialized || !out || s->error)return false;
    /* FM retains its original64-sample native partitions internally. Batch
     * four of those before PCM so its312-byte fast-state copy is paid once
     * per source boundary/refill, rather than on every FM block. A256-frame
     * batch spans less than one source tick, keeping the pending PCM queue
     * bounded. No source writes occur between these synchronous operations.
     * This owner is non-reentrant; keep the1KiB scratch off the IRQ stack. */
    static int16_t pcm[512];
    uint32_t digest=2166136261u;
    while(samples){
        unsigned count=samples>256?256:samples;
        if(!forecourt_fm_render(out,count) || s->error){s->error=5;return false;}
        unsigned done=0;
        while(done<count){
            uint64_t next=resource_shared_sound_boundary(s->next_frame);
            if(s->samples==next){
                if(!tick_pcm(s))return false;
                next=resource_shared_sound_boundary(s->next_frame);
            }
            if(next<s->samples){s->error=6;return false;}
            unsigned n=next-s->samples>count-done?count-done:(unsigned)(next-s->samples);
            resource_pcm_output_render_masked(&s->pcm.output,&s->pcm.driver.chip,pcm,n,settings_pcm_mask(s->pcm.driver.ram[0x11]==2 || s->pcm.driver.ram[0x11]==0x82));
            if(hash){
#ifdef __NDS__
                extern uint32_t shared_mix_hash_arm(int16_t *,const int16_t *,unsigned,uint32_t);
                digest=shared_mix_hash_arm(out+done*2,pcm,n,digest);
#else
                for(unsigned i=0;i<n*2;i++){
                    int value=out[done*2+i]+pcm[i];
                    value=value>32767?32767:value< -32768?-32768:value;
                    out[done*2+i]=value;
                    digest=(digest^(uint16_t)value)*16777619u;
                }
#endif
            }else{
#ifdef __NDS__
                extern void shared_mix_arm(int16_t *,const int16_t *,unsigned);
                shared_mix_arm(out+done*2,pcm,n);
#else
                for(unsigned i=0;i<n*2;i++){
                    int value=out[done*2+i]+pcm[i];
                    out[done*2+i]=value>32767?32767:value< -32768?-32768:value;
                }
#endif
            }
            done+=n;s->samples+=n;
        }
        out+=count*2;samples-=count;
    }
    if(hash)*hash=digest;
    return true;
}
bool resource_shared_sound_render(resource_shared_sound *s,int16_t *out,unsigned samples){
    return render_output(s,out,samples,NULL);
}
bool resource_shared_sound_render_observed(resource_shared_sound *s,int16_t *out,unsigned samples,uint32_t *hash){
    return hash && render_output(s,out,samples,hash);
}
void resource_shared_sound_generated_status(const resource_shared_sound *s,uint8_t *music,uint8_t *effect){
    unsigned m=s->music,e=s->effect,sequence=s->sequence;
    for(unsigned i=0;i<s->notice_count;i++){
        const resource_sound_notification *n=s->notices+((s->notice_read+i)&RESOURCE_SHARED_SOUND_NOTICE_MASK);
        if((n->signal&15)!=sequence){
            if((n->signal&128) && n->frame>=s->music_request)m=0;
            if((n->signal&64) && n->frame>=s->effect_request)e=0;
            sequence=n->signal&15;
        }
    }
    *music=m;*effect=e;
}
bool resource_shared_sound_played(resource_shared_sound *s,uint64_t samples){
    if(!s || shared_owner!=s || !s->initialized || s->error || samples<s->played || samples>s->samples)return false;
    while(s->notice_count){
        const resource_sound_notification *n=s->notices+s->notice_read;
        if(resource_shared_sound_boundary(n->frame)>samples)break;
        unsigned sequence=n->signal&15;
        /* Original SUB DF52: only a changed low-nibble sequence consumes
         * bits7/6. An old buffered completion cannot stop a newer request. */
        if(sequence!=s->sequence){
            if((n->signal&128) && n->frame>=s->music_request)s->music=0;
            if((n->signal&64) && n->frame>=s->effect_request)s->effect=0;
            s->sequence=sequence;
        }
        s->notice_read=(s->notice_read+1)&RESOURCE_SHARED_SOUND_NOTICE_MASK;s->notice_count--;
    }
    s->played=samples;return true;
}

#include "resource_shared_sound_state.inc"
