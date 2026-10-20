#include "resource_pcm_output.h"
#include <string.h>
#include "resource_pcm_batch_clock.h"
#define TIME_UNIT (UINT64_C(1)<<52)
bool resource_pcm_output_reset(resource_pcm_output *s,unsigned rate){
    if(!s || rate<8000 || rate>48000)return false;
    memset(s,0,sizeof(*s));s->rate=rate;
    /* Round only at initialization, in the same52-bit clock domain as
     * the reference output path. Integer sample processing follows. */
    double factor=(double)TIME_UNIT*(double)rate/(50000000.0/1536.0);
    s->factor=(uint64_t)factor;
    if((double)s->factor<factor)s->factor++;
    s->next=s->factor/2;return true;
}
#ifndef __NDS__
static int32_t pcm_clip(int64_t value){
#if (-1 >> 1) != -1
#error PCM output requires arithmetic signed right shift
#endif
    /* Unsigned rotation maps exactly[-2^30,2^30-1] to[0,2^31-1].
     * Outside that interval the final16-bit DAC sample saturates. The
     * integrator itself retains all64 bits, including filter overshoot. */
    if((uint64_t)value+(UINT64_C(1)<<30)>=(UINT64_C(1)<<31))
        return value<0?-32768:32767;
    return (int32_t)value>>15;
}

#endif

#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static __attribute__((noinline)) void pcm_output_render_hot(resource_pcm_output *s,resource_pcm_chip *chip,int16_t *out,size_t frames){
    if((!chip->enabled || chip->off==255) && !s->previous[0] && !s->previous[1] &&
       s->integrator[0]>=0 && s->integrator[0]<32768 && s->integrator[1]>=0 && s->integrator[1]<32768){
        bool quiet=true;
        for(unsigned i=0;i<32;i++)if(s->delta[i][0] || s->delta[i][1]){quiet=false;break;}
        if(quiet){
            /* Silent hardware still advances its exact fractional clock.
             * No channel position or filter tail is discarded. Bound each
             * block so52-bit fixed-point products cannot overflow64 bits. */
            memset(out,0,frames*4);
            while(frames){
                unsigned n=frames>1024?1024:frames;
                uint64_t duration=TIME_UNIT*n;
                uint64_t clocks=duration>s->next?(duration-s->next+s->factor-1)/s->factor:0;
                s->next+=clocks*s->factor-duration;s->clocks+=clocks;
                s->cursor=(s->cursor+n)&31;frames-=n;
            }
            return;
        }
    }
    /* Batch only the independent chip samples between caller-scheduled
     * register writes. The52-bit resampling clock and filter order stay exact.
     * At the supported minimum8kHz,64 output frames need at most261 clocks. */
    static int16_t raw[272][2];
    while(frames){
        unsigned count=frames>64?64:(unsigned)frames;
        unsigned clocks=resource_pcm_batch_clocks(s->factor,s->next,count);
        resource_pcm_chip_run(chip,&raw[0][0],clocks);
#ifdef __NDS__
        extern unsigned pcm_output_filter_select(resource_pcm_output *,const int16_t *,int16_t *,unsigned,unsigned);
        unsigned cursor=pcm_output_filter_select(s,&raw[0][0],out,count,clocks);
#else
        unsigned cursor=0;
    for(unsigned n=0;n<count;n++){
        while(s->next<TIME_UNIT){
            unsigned fraction=(s->next>>37)&32767;
            for(unsigned c=0;c<2;c++){
                int difference=(int)raw[cursor][c]-s->previous[c];s->previous[c]=raw[cursor][c];
                s->delta[(s->cursor+7)&31][c]+=difference*(32768-(int)fraction);
                s->delta[(s->cursor+8)&31][c]+=difference*(int)fraction;
            }
            s->next+=s->factor;cursor++;
        }
        for(unsigned c=0;c<2;c++){
            int32_t sample=pcm_clip(s->integrator[c]);
            s->integrator[c]+=s->delta[s->cursor][c];s->delta[s->cursor][c]=0;
            int32_t correction=sample*64;
            s->integrator[c]-=correction;out[n*2+c]=sample;
        }
        s->cursor=(s->cursor+1)&31;s->next-=TIME_UNIT;
    }
#endif
        /* The clock counter is observational, not part of sample generation.
         * Commit its exact increment once per batch instead of every clock. */
        s->clocks+=cursor;
        out+=count*2;frames-=count;
    }
}

/* A single synchronous refill owns this scratch state. Preserve the public
 * room/checkpoint ABI and copy every field back before exposing the result. */
#include "audio_hot_state.h"
#define pcm_hot_state audio_hot_state.pcm
void resource_pcm_output_render(resource_pcm_output *s,resource_pcm_chip *chip,int16_t *out,size_t frames){
 pcm_hot_state=*s;
 pcm_output_render_hot(&pcm_hot_state,chip,out,frames);
 *s=pcm_hot_state;
}
