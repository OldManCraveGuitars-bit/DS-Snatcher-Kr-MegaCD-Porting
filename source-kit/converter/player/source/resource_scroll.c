#include "resource_scroll.h"
#include <string.h>
static uint16_t word(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static void putword(uint8_t *p,uint16_t v){p[0]=v>>8;p[1]=v;}
static void putlong(uint8_t *p,uint32_t v){putword(p,v>>16);putword(p+2,v);}
void resource_scroll_mode(resource_scroll_state *s,unsigned mode){
    mode&=255;s->changed=1;
    if(mode==255 || mode==252){
        memset(s->control,0,sizeof(s->control));memset(s->output,0,sizeof(s->output));
        putword(s->control+12,mode);return;
    }
    if(mode>=7 && mode!=253){s->control[10]=mode;return;}
    s->control[11]=mode;
    if(mode>=3){putword(s->control,0);return;}
    putword(s->control,mode);if(mode==1)s->control[0]=1;
}
bool resource_scroll_actor(resource_scroll_state *s,unsigned op,unsigned arg,uint16_t value){
    if(!s || op<0x8c || op>0x8f)return false;
    unsigned at;
    if(op<0x8e){resource_scroll_mode(s,1);at=op==0x8c?0x18:0x1c;if(arg)at+=16;}
    else{resource_scroll_mode(s,2);at=op==0x8e?2:4;if(arg)at+=4;}
    putword(s->control+at,value);return true;
}
void resource_scroll_tick(resource_scroll_state *s,uint32_t fill){
    uint8_t *c=s->control;
    if(word(c+12)){
        for(unsigned i=0;i<24;i+=4)putlong(s->shared+i,fill);
        s->shared[4]=(uint8_t)word(c+12);putword(c+12,0);return;
    }
    if(c[10])s->shared[0]=c[10];
    if(c[11])s->shared[4]=c[11];
    putword(c+10,0);
    if(!word(c))return;
    uint32_t position[4];
    for(unsigned i=0;i<4;i++){
        unsigned at=i<2?0x18+i*4:0x28+(i-2)*4;
        position[i]=longword(c+at);putlong(c+at,0);
        if(c[0]!=1)position[i]+=longword(c+0x30+i*4);
        position[i]+=(uint32_t)((int32_t)(int16_t)word(c+2+i*2)*256);
    }
    if(c[0]==1)c[0]=0;
    for(unsigned i=0;i<4;i++){
        putlong(c+0x30+i*4,position[i]);putword(s->output+i*2,position[i]>>16);
    }
}
