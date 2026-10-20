#include "resource_pcm_chip.h"
#include <string.h>
#ifdef __NDS__
extern uint32_t pcm_channel_mix_arm(const uint8_t *,uint32_t,unsigned,unsigned,
    int *,unsigned,unsigned,unsigned) __attribute__((long_call));
extern uint32_t pcm_channel_group_arm(const uint8_t *,uint32_t,unsigned,unsigned,
    int *,unsigned,unsigned,unsigned,unsigned) __attribute__((long_call));
#endif

void resource_pcm_chip_reset(resource_pcm_chip *s){
    memset(s,0,sizeof(*s));s->off=255;
    for(unsigned i=0;i<8;i++)s->registers[i][1]=255;
}
void resource_pcm_chip_write(resource_pcm_chip *s,unsigned reg,uint8_t v){
    if(reg>=0x1000){s->ram[s->bank*4096+(reg&4095)]=v;return;}
    if(reg<7){
        s->registers[s->channel][reg]=v;
        if(reg==6 && (s->off&(1u<<s->channel)))s->position[s->channel]=(uint32_t)v<<19;
    }else if(reg==7){
        s->enabled=v>>7;
        if(v&64)s->channel=v&7;else s->bank=v&15;
    }else if(reg==8){
        s->off=v;
        for(unsigned i=0;i<8;i++)if(v&(1u<<i))s->position[i]=(uint32_t)s->registers[i][6]<<19;
    }
}
uint8_t resource_pcm_chip_read(const resource_pcm_chip *s,unsigned reg){
    if(reg>=0x1000)return s->ram[s->bank*4096+(reg&4095)];
    if(reg>=16 && reg<32)return s->position[(reg-16)/2]>>((reg&1)?19:11);
    return 255;
}
static int scaled(int sample,unsigned envelope,unsigned pan){
    int value=sample*(int)(envelope*pan);
    /* Both supported GCC targets use arithmetic signed shifts. Require that
     * contract explicitly; it is the same floor division for negative DAC
     * samples without the per-channel negate/round/negate sequence. */
#if (-1 >> 1) != -1
#error PCM DAC requires arithmetic signed right shift
#endif
    return value>>5;
}
#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
void resource_pcm_chip_run(resource_pcm_chip *s,int16_t *out,size_t frames){
    /* Registers are constant until the caller's next original driver tick.
     * Decode them once per channel/block, preserving every native sample and
     * the original marker rule (jumping to a loop does not advance the step). */
    /* Synchronous chip mixing uses the ARM9's reserved fast stack. Keeping
     * this scratch out of the four-KiB data cache avoids evicting waveform
     * bytes on every channel. It contains no persistent chip state. */
    int mixed[128][2];
#ifdef __NDS__
    extern void fm_observe_stack(void);fm_observe_stack();
#endif
    while(frames){
        unsigned count=frames>128?128:(unsigned)frames;
        if(out)memset(mixed,0,count*sizeof(*mixed));
        unsigned active=s->enabled?(~s->off)&255:0;
        while(active){
            unsigned i=__builtin_ctz(active);active&=active-1;
            const uint8_t *r=s->registers[i];uint32_t position=s->position[i];
            unsigned step=r[2]|(unsigned)r[3]<<8,loop=r[4]|(unsigned)r[5]<<8;
            unsigned left=r[0]*(r[1]&15),right=r[0]*(r[1]>>4);
            unsigned paired=0,multiplicity=1;
            for(unsigned bits=active;bits;bits&=bits-1){
                unsigned j=__builtin_ctz(bits);const uint8_t *other=s->registers[j];
                if(s->position[j]==position &&
                   (other[2]|(unsigned)other[3]<<8)==step &&
                   (other[4]|(unsigned)other[5]<<8)==loop &&
                   other[0]*(other[1]&15)==left && other[0]*(other[1]>>4)==right){
                    paired|=1u<<j;active&=~(1u<<j);multiplicity++;
                }
            }
#ifdef __NDS__
            if(out){
                if(multiplicity==1)position=pcm_channel_mix_arm(s->ram,position,step,loop,
                    &mixed[0][0],count,left,right);
                else position=pcm_channel_group_arm(s->ram,position,step,loop,
                    &mixed[0][0],count,left,right,multiplicity);
            }else
#endif
            for(unsigned n=0;n<count;n++){
                unsigned value=s->ram[(position>>11)&65535];
                if(value==255){position=(uint32_t)loop<<11;value=s->ram[loop];}
                else position+=step;
                if(out && value!=255){
                    int sample=(value&128)?(int)(value&127):-(int)value;
                    mixed[n][0]+=scaled(sample,left,1)*(int)multiplicity;
                    mixed[n][1]+=scaled(sample,right,1)*(int)multiplicity;
                }
            }
            s->position[i]=position;
            while(paired){
                unsigned j=__builtin_ctz(paired);paired&=paired-1;s->position[j]=position;
            }
        }
        if(out)for(unsigned n=0;n<count;n++)for(unsigned c=0;c<2;c++){
            int value=mixed[n][c];out[n*2+c]=value>32767?32767:value< -32768?-32768:value;
        }
        if(out)out+=count*2;
        frames-=count;
    }
}
