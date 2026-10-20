#include "resource_sprites.h"
#include <string.h>
static uint16_t word(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static void putword(uint8_t *p,uint16_t v){p[0]=v>>8;p[1]=v;}
static uint16_t rotate(uint16_t v,unsigned n){return (uint16_t)(v>>n|v<<(16-n));}
bool resource_sprites_build_owned(const uint8_t actors[64*80],resource_graphics_source source,
    void *ctx,uint16_t frame,const uint16_t scroll[4],uint8_t sat[64*8],unsigned *count,uint8_t owners[64],uint32_t maps[64]){
    if(!actors || !source || !scroll || !sat || !count)return false;
    *count=0;memset(sat,0,8);
    for(unsigned id=0;id<64;id++){
        const uint8_t *a=actors+id*80;uint16_t blink=word(a+2);uint32_t map=longword(a+0x36);
        if(!word(a) || !map || (a[0x25]&2) || blink==65535 || (blink&frame))continue;
        size_t remaining=0;const uint8_t *p=source(ctx,map,&remaining);if(!p || remaining<2)return false;
        unsigned entries=word(p)+1;p+=2;remaining-=2;
        uint16_t x=word(a+6)+128,y=word(a+10)+128;unsigned flags=a[5];
        if(flags&4){x+=scroll[0];y+=scroll[1];}
        if(flags&8){x+=scroll[2];y+=scroll[3];}
        uint16_t flips=rotate(word(a+0x18)&0xfc,3),attributes=rotate(flags&3,5);
        for(unsigned i=0;i<entries;i++,p+=8,remaining-=8){
            if(remaining<8)return false;
            uint16_t dy=word(p),size=word(p+2),attr=word(p+4),dx=word(p+6);
            if(flags&2)dy=-dy-(((size>>5)&24)+8);
            if(flags&1)dx=-dx-(((size>>7)&24)+8);
            attr=(attr|flips)^attributes;
            if(a[0x16]&128)attr=(attr&0x9fff)|rotate(a[0x16]&3,3);
            uint8_t *out=sat+(*count)*8;putword(out,y+dy);putword(out+2,size+*count+1);putword(out+4,attr);putword(out+6,x+dx);
            if(owners)owners[*count]=(uint8_t)id;
            if(maps)maps[*count]=longword(a+0x2e);
            if(++*count==64){out[3]=0;return true;}
        }
    }
    if(*count)sat[(*count-1)*8+3]=0;
    return true;
}

bool resource_sprites_build(const uint8_t actors[64*80],resource_graphics_source source,
    void *ctx,uint16_t frame,const uint16_t scroll[4],uint8_t sat[64*8],unsigned *count){
    return resource_sprites_build_owned(actors,source,ctx,frame,scroll,sat,count,NULL,NULL);
}
