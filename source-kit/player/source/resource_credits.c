#include "resource_credits.h"
#include <string.h>

static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static unsigned handler(const uint8_t *data,unsigned i){
    return 0x290fc+(int16_t)word(data+0x10fc+i*6);
}
static bool known(unsigned pc){
    switch(pc){
    case 0x291d4:case 0x291da:case 0x291e0:case 0x291e6:case 0x291ec:
    case 0x291f2:case 0x291f8:case 0x291fe:case 0x29204:case 0x2920a:
    case 0x2923a:case 0x29250:case 0x2925c:case 0x29266:case 0x29280:
    case 0x2928c:case 0x29298:case 0x292ae:case 0x292bc:case 0x292e6:
    case 0x292f4:case 0x29310:return true;
    default:return false;
    }
}
bool resource_credits_bind(resource_credits_state *s,const void *data,size_t bytes){
    static const uint8_t dispatch[]={0x30,0x38,0x78,0x96,0x6b,0x00,0x00,0xca,
        0xd0,0x40,0x32,0x00,0xd0,0x40,0xd0,0x41,0x22,0x3b,0x00,0x18,
        0xb2,0xb8,0x78,0x7a,0x63,0x02,0x4e,0x75,0x52,0x78,0x78,0x96,
        0x30,0x3b,0x00,0x06,0x4e,0xfb,0x00,0x02};
    if(!s)return false;
    memset(s,0,sizeof(*s));
    const uint8_t *p=data;
    if(!p || bytes!=182794 || longword(p+8)!=0x290d4 ||
       memcmp(p+0x10d4,dispatch,sizeof(dispatch)))return false;
    for(unsigned i=0;i<28;i++){
        if(!known(handler(p,i)))return false;
        uint32_t clock=longword(p+0x10fe +i*6);
        if(clock&255)return false;
        for(unsigned shift=8;shift<=24;shift+=8){
            unsigned v=(clock>>shift)&255;
            if((v&15)>9 || (v>>4)>9 || (shift==16 && v>=0x60) || (shift==8 && v>=0x75))return false;
        }
    }
    s->data=p;s->bytes=bytes;s->index=-1;return true;
}
static int palette(resource_callback_job job,void *ctx,uint32_t at){
    return job(ctx,3,0,0,at)?1:-1;
}
static int scene(resource_callback_job job,void *ctx,unsigned id){
    return job(ctx,1,0,0,0) && job(ctx,2,0,0,id)?1:-1;
}
int resource_credits_tick(resource_credits_state *s,uint32_t clock,uint16_t status,
    uint8_t special,uint8_t input,resource_callback_job job,void *ctx){
    if(!s || !s->data || !job)return -1;
    s->clock=clock;
    if(s->finished)return 0;
    if(s->index<0){
        if(!s->phase){s->phase++;return job(ctx,7,0,0,19)?1:-1;}
        if(status==0x100){s->clock=0;s->phase=0;s->index=0;}
        return 1;
    }
    if(s->index>=28)return -1;
    unsigned i=(unsigned)s->index;
    if(longword(s->data+0x10fe +i*6)>clock)return 0;
    s->index++;
    unsigned pc=handler(s->data,i);
    if(pc>=0x291d4 && pc<=0x29204 && (pc-0x291d4)%6==0)
        return scene(job,ctx,(pc-0x291d4)/6);
    switch(pc){
    case 0x2920a:
        if(!(status&0xff00) || (status&0xff00)==0x500){
            s->counter=300;if(special)s->index=24;
        }else s->index--;
        return 1;
    case 0x2923a:
        s->counter--;
        if(s->counter){s->index--;return 1;}
        s->counter=64;return palette(job,ctx,0x292e2);
    case 0x29250:case 0x29280:case 0x292ae:
        s->counter--;if(s->counter)s->index--;return 1;
    case 0x2925c:return job(ctx,7,0,0,2)?1:-1;
    case 0x29266:
        if(status==0x100)s->counter=24;else s->index--;
        return 1;
    case 0x2928c:s->counter=300;return scene(job,ctx,9);
    case 0x29298:
        if(!(input&128)){s->index--;return 1;}
        s->counter=64;return palette(job,ctx,0x292e2);
    case 0x292bc:s->finished=1;s->index=0;return 1;
    case 0x292e6:return palette(job,ctx,0x292f0);
    case 0x292f4:return palette(job,ctx,0x292fe);
    case 0x29310:return palette(job,ctx,0x2931a);
    default:return -1;
    }
}
