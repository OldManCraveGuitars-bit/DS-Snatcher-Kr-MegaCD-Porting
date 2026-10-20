#include "resource_graphics.h"
#include "resource_lzkn1.h"
#include <string.h>
static uint16_t word(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static void putlong(uint8_t *p,uint32_t value){p[0]=value>>24;p[1]=value>>16;p[2]=value>>8;p[3]=value;}
static const uint8_t *read(resource_graphics_source source,void *ctx,uint32_t at,size_t n){
    size_t left=0;const uint8_t *p=source(ctx,at,&left);return p && left>=n?p:NULL;
}
void resource_graphics_clear(resource_graphics_state *s){if(s)memset(s->jobs,0,sizeof(s->jobs));}
bool resource_graphics_enqueue(resource_graphics_state *s,resource_graphics_source source,void *ctx,uint32_t at){
    if(!s || !source)return false;
    for(unsigned i=0;i<7;i++){
        uint8_t *j=s->jobs+i*8;if(j[0])continue;
        const uint8_t *p=read(source,ctx,at,2);if(!p){s->error=1;return false;}
        j[0]=p[0];j[1]=0;putlong(j+2,at+2);return true;
    }
    return true; /* Native full queue ignores an additional command. */
}
static bool decode(resource_graphics_state *s,resource_graphics_source source,void *ctx,uint32_t at,unsigned bank){
    size_t left=0;const uint8_t *p=source(ctx,at,&left);size_t written=0,consumed=0;
    return p && resource_lzkn1(p,left,s->scratch+bank*8192,8192,&written,&consumed);
}
bool resource_graphics_enqueue_health(resource_graphics_state *s,resource_graphics_source source,void *ctx,uint16_t destination){
    const uint8_t *p=read(source,ctx,0x12ea8,14);
    if(!s || !p || word(p)!=0x0200 || word(p+10)!=6 || word(p+12)!=65535)return false;
    if(!resource_graphics_enqueue(s,source,ctx,0x12ea8))return false;
    for(unsigned i=0;i<7;i++){
        uint8_t *j=s->jobs+i*8;
        if(j[0]==2 && longword(j+2)==0x12eaa){j[1]=1;j[6]=destination>>8;j[7]=destination;}
    }
    return true;
}
static bool transfer(resource_graphics_state *s,resource_graphics_dma dma,void *ctx,const uint8_t *record,unsigned bank){
    int offset=(int16_t)word(record+6);unsigned n=word(record+8);
    int start=(int)bank*8192+offset;
    if(start<0 || (unsigned)start>sizeof(s->scratch) || n>sizeof(s->scratch)-(unsigned)start)return false;
    return dma(ctx,0x238000+start,word(record+4),s->scratch+start,n);
}
static bool step(resource_graphics_state *s,uint8_t *j,resource_graphics_source source,
    resource_graphics_dma dma,void *ctx,bool busy,uint16_t zero_word){
    unsigned bank=(j[0]>>4)&3,kind=j[0]&15;s->bank=0xb8000+bank*8192;
    if(kind<1 || kind>5){s->error=2;return false;}
    if(!bank && busy)return true;
    uint32_t at=longword(j+2);
    if(kind==5){
        uint8_t *scratch=s->scratch+bank*8192;
        const uint8_t *lut=read(source,ctx,0xea78,16);if(!lut)return false;
        if(!j[1])memset(scratch,0,32);
        unsigned column=j[1]&7,index=(((j[1]>>3)*5)+column)&7;
        scratch[column*4+lut[index*2]]|=(uint8_t)(~lut[index*2+1])&0x77;
        if(!dma(ctx,s->bank+0x180000,zero_word,scratch,32))return false;
        if(++j[1]==65)j[0]=0;
        return true;
    }
    const uint8_t *p=read(source,ctx,at,10);if(!p)return false;
    if(kind==2){
        for(unsigned budget=0;budget<8192;budget++){
            uint8_t packet[10];
            if(j[1]){
                if(j[1]!=1 || at!=0x12eaa)return false;
                memcpy(packet,p,10);packet[4]=j[6];packet[5]=j[7];p=packet;
            }
            unsigned n=word(p+8);int start=(int)bank*8192+(int16_t)word(p+6);
            const uint8_t *raw=read(source,ctx,longword(p),n);
            if(!raw || start<0 || (unsigned)start>sizeof(s->scratch) || n>sizeof(s->scratch)-(unsigned)start)return false;
            memmove(s->scratch+start,raw,n);
            if(!transfer(s,dma,ctx,p,bank))return false;
            at+=10;putlong(j+2,at);p=read(source,ctx,at,2);if(!p)return false;
            if(word(p)==65535){j[0]=0;return true;}
            p=read(source,ctx,at,10);if(!p)return false;
        }
        return false;
    }
    if(!(j[0]&128)){
        if(!decode(s,source,ctx,longword(p),bank))return false;
        if(kind==4)j[0]|=128;
    }
    if(kind==4){
        const uint8_t *lut=read(source,ctx,0xea78,16);if(!lut)return false;
        unsigned column=j[1]&7,index=(((j[1]>>3)*5)+column)&7,n=word(p+8);
        if(n<32 || (n&31))return false;
        int first=(int)bank*8192+(int16_t)word(p+6)+column*4+lut[index*2];
        unsigned stride=(n>>5)-1;
        if(first<0 || (unsigned)first>=sizeof(s->scratch) || stride*32>=sizeof(s->scratch)-(unsigned)first)return false;
        for(unsigned i=0;i<=stride;i++)s->scratch[first+i*32]&=lut[index*2+1];
        if(!transfer(s,dma,ctx,p,bank))return false;
        if(++j[1]==65)j[0]=0;
        return true;
    }
    for(unsigned budget=0;budget<8192;budget++){
        if(!transfer(s,dma,ctx,p,bank))return false;
        at+=10;putlong(j+2,at);const uint8_t *end=read(source,ctx,at,2);if(!end)return false;
        if(word(end)==65535){j[0]=0;return true;}
        if(kind==1)return true;
        at-=4;putlong(j+2,at);p=read(source,ctx,at,10);if(!p)return false;
    }
    return false;
}
bool resource_graphics_tick(resource_graphics_state *s,resource_graphics_source source,
    resource_graphics_dma dma,void *ctx,bool freeze,bool busy,uint16_t zero_word){
    if(!s || !source || !dma)return false;
    s->error=0;if(freeze)return true;
    for(unsigned i=0;i<7;i++){
        uint8_t *j=s->jobs+i*8;unsigned kind=j[0];if(!kind)continue;
        if(!step(s,j,source,dma,ctx,busy,zero_word)){if(!s->error)s->error=1;return false;}
        if(!(kind&64))break;
    }
    return true;
}
