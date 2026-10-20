#include "resource_main_scroll.h"
#include <string.h>
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static void putword(uint8_t *p,unsigned value){p[0]=value>>8;p[1]=value;}
static void band(resource_cards_state *s,uint8_t *lines,unsigned index){
    static const unsigned starts[]={0x22,0x42,0x66,0x96,0xbe,0xde,0xf6,0x10a,0x11a};
    static const unsigned counts[]={8,9,12,10,8,6,5,4,3};
    static const unsigned last[]={7,16,28,38,46,52,57,61,64};
    putword(s->main+0x7a,last[index]);
    for(unsigned row=0;row<counts[index];row++){
        uint8_t *p=lines+starts[index]+row*4;
        putword(p,(word(p)-1)&1023);
    }
}
static void interval(resource_cards_state *s,uint8_t *lines,
    unsigned counter,unsigned ticks,unsigned index){
    if(word(s->main+counter)==ticks){putword(s->main+counter,0);band(s,lines,index);}
}
static void shift_band(resource_cards_state *s,uint8_t *lines,
    unsigned start,unsigned count,unsigned last,int delta){
    putword(s->main+0x7a,last);
    for(unsigned row=0;row<count;row++){
        uint8_t *p=lines+start+row*4;
        putword(p,(word(p)+delta)&1023);
    }
}
static void initialize(resource_cards_state *s,uint8_t *lines){
    uint8_t *c=s->main;
    putword(c+0x78,c[0x70]);putword(c+0x70,0);
    putword(c+0x7a,127);putword(c+0x76,0);
    memset(c+0x7c,0,10);memset(lines+0x22,0,512);
    putword(s->cached_registers+6,(word(s->cached_registers+6)&0xfff8)|3);
}
static void fill_band(uint8_t *lines,unsigned start,unsigned count,unsigned value){
    for(unsigned row=0;row<count;row++)putword(lines+start+row*4,value);
}
static bool pulse(resource_cards_state *s,uint8_t *lines,resource_cards_port port,void *ctx){
    uint8_t *c=s->main;
    unsigned phase=word(c+0x86);
    switch(phase){
    case 0:putword(c+0x86,1);initialize(s,lines);return true;
    case 1:
        putword(c+0x86,2);putword(c+0x7a,111);
        fill_band(lines,0xe2,3,0x3f8);fill_band(lines,0xee,2,0x3f4);
        fill_band(lines,0xf6,3,0x3f8);fill_band(lines,0x142,24,2);
        fill_band(lines,0x1a2,16,4);return true;
    case 2:case 3:case 4:{
        unsigned at=0x7c+(phase-2)*2;
        unsigned ticks=phase==2?8:phase==3?13:7;
        unsigned counter=(word(c+at)+1)&65535;putword(c+at,counter);
        /* The source uses BMI after CMP.W, including signed wrap. */
        if((counter-ticks)&0x8000)return true;
        if(phase<4){
            unsigned reg=word(s->cached_registers+6)&0xfff8;
            putword(s->cached_registers+6,reg|(phase==3?3:0));
            putword(c+0x86,phase+1);return true;
        }
        if(!port || !port(ctx,0xc00004,2,word(s->cached_registers)))return false;
        /* FF3406..FF3458 restores normal scrolling and clears both effects. */
        memset(c+0x60,0,26);memset(c+0x86,0,6);
        putword(s->cached_registers+6,word(s->cached_registers+6)&0xfff8);
        memset(lines,0,4);memset(s->vscroll,0,4);memset(s->hscroll,0,4);
        return true;
    }
    default:return false;
    }
}
bool resource_main_scroll_tick(resource_cards_state *s,uint8_t lines[896],uint16_t frame,
    resource_cards_port port,void *ctx){
    if(!s || !lines)return false;
    uint8_t *c=s->main;
    unsigned mode=word(c+0x78);if(!mode)mode=c[0x70];
    if(mode<3 || mode>6)return false;
    if(mode==5)return pulse(s,lines,port,ctx);
    if(!word(c+0x78)){
        initialize(s,lines);return true;
    }
    if(mode==6){
        shift_band(s,lines,0x1e2,9,120,-2);
        shift_band(s,lines,0x206,7,127,2);return true;
    }
    for(unsigned at=0x7c;at<=0x84;at+=2)putword(c+at,word(c+at)+1);
    if(mode==4){
        static const unsigned starts[]={0x22,0x1e2,0x1f2,0x202,0x212};
        for(unsigned i=0;i<5;i++)if(word(c+0x7c+i*2)==7-i){
            putword(c+0x7c+i*2,0);shift_band(s,lines,starts[i],i?4:112,111+i*4,1);
        }
        return true;
    }
    if(!(frame&1))band(s,lines,0);
    if(!(frame&3))band(s,lines,1);
    interval(s,lines,0x7c,6,2);
    if(!(frame&7))band(s,lines,3);
    interval(s,lines,0x7e,10,4);
    interval(s,lines,0x80,12,5);
    interval(s,lines,0x82,14,6);
    if(!(frame&15))band(s,lines,7);
    interval(s,lines,0x84,18,8);
    return true;
}
