#include "resource_pcm_output.h"
extern unsigned pcm_output_filter_arm(resource_pcm_output *,const int16_t *,int16_t *,unsigned);
extern unsigned pcm_output_filter_mono_arm(resource_pcm_output *,const int16_t *,int16_t *,unsigned);
volatile unsigned pcm_mono_blocks,pcm_stereo_blocks;
unsigned pcm_output_filter_select(resource_pcm_output *s,const int16_t *raw,int16_t *out,unsigned frames,unsigned clocks){
    /* Conservative, complete equality: stereo tails must finish before a
     * newly centred voice can share calculations. No pan/register guess. */
    bool same=s->previous[0]==s->previous[1] && s->integrator[0]==s->integrator[1];
    if(same){
        unsigned difference=0;
        for(unsigned i=0;i<32;i++)difference|=(unsigned)s->delta[i][0]^(unsigned)s->delta[i][1];
        for(unsigned i=0;i<clocks;i++)difference|=(unsigned)(uint16_t)raw[i*2]^(unsigned)(uint16_t)raw[i*2+1];
        same=!difference;
    }
    if(same){pcm_mono_blocks++;return pcm_output_filter_mono_arm(s,raw,out,frames);}
    pcm_stereo_blocks++;return pcm_output_filter_arm(s,raw,out,frames);
}
