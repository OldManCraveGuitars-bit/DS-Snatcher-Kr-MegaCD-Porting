#include "resource_palette.h"
#include <string.h>
static uint16_t word(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static void put_long(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
typedef struct {const uint8_t *data;size_t size;uint32_t base;} single_source;
static const uint8_t *single(void *ctx,uint32_t at,size_t *remaining){
    const single_source *s=ctx;*remaining=0;
    if(at<s->base || at-s->base>s->size)return NULL;
    *remaining=s->size-(at-s->base);return s->data+at-s->base;
}
static const uint8_t *address(resource_palette_source source,void *ctx,uint32_t at,size_t n){
    size_t remaining=0;const uint8_t *data=source(ctx,at,&remaining);
    return data && n<=remaining?data:NULL;
}
void resource_palette_reset(resource_palette_state *s){memset(s,0,sizeof(*s));}
bool resource_palette_enqueue_resolved(resource_palette_state *s,resource_palette_source source,void *ctx,uint32_t command){
    if(!s || !source || s->cursor>=12)return false;
    const uint8_t *c=address(source,ctx,command,4);
    if(!c || !(c[0]&127) || (c[0]&127)>10 || (c[2]&1) || c[2]/2+c[3]+1>128){s->error=1;return false;}
    for(unsigned i=1;i<=12;i++){
        uint8_t *j=s->jobs+((s->cursor+i)%12)*16;
        if(j[0])continue;
        put_long(j+8,command);j[0]=c[0];j[1]=0;j[2]=c[2];j[3]=c[3];put_long(j+4,command+4);j[12]=0;
        if(c[0]&128)for(unsigned k=0;k<12;k++)if(s->jobs[k*16]==6)s->jobs[k*16]=0;
        j[0]&=127;return true;
    }
    /* Native enqueue returns without changing jobs when all12 slots are busy. */
    return true;
}
static uint16_t toward(uint16_t current,uint16_t target){
    unsigned result=0;
    for(unsigned shift=0;shift<=8;shift+=4){
        unsigned c=(current>>shift)&15,t=(target>>shift)&15;
        if(c<t)c=(c+2)&15;else if(c>t)c=(c-2)&15;
        result|=c<<shift;
    }
    return result;
}
bool resource_palette_tick_resolved(resource_palette_state *s,resource_palette_source source,void *ctx,bool freeze){
    if(!s || !source || s->cursor>=12)return false;
    if(freeze)return true;
    for(unsigned i=1;i<=12;i++){
        uint8_t *j=s->jobs+((s->cursor+i)%12)*16;
        unsigned kind=j[0];if(!kind)continue;
        if(kind>10 || (j[2]&1) || j[2]/2+j[3]+1>128){s->error=1;return false;}
        unsigned first=j[2]/2,count=j[3]+1;
        uint16_t *palette=s->colors+first;
        if(kind==2 || kind==4){
            for(unsigned n=0;n<count;n++)palette[n]=kind==2?0:0xeee;
            j[0]=kind=9;
        }
        if(j[1]){j[1]--;continue;}
        const uint8_t *command=address(source,ctx,longword(j+8),4);
        if(!command){s->error=1;return false;}
        j[1]=command[1];s->changed|=j[2]&128?2:1;
        const uint8_t *pointer=NULL,*target=NULL;
        if(kind==1 || kind==6 || kind==7 || kind==8 || kind==9){
            pointer=address(source,ctx,longword(j+4),4);
            if(pointer)target=address(source,ctx,longword(pointer),kind==7?2:count*2);
            if(!target){s->error=1;return false;}
        }
        if(kind==1 || kind==6){
            for(unsigned n=0;n<count;n++)palette[n]=word(target+n*2);
            uint32_t next=longword(j+4)+4;put_long(j+4,next);
            const uint8_t *end=address(source,ctx,next,2);
            if(!end){s->error=1;return false;}
            if(word(end)==65535){if(kind==1)j[0]=j[1]=0;else put_long(j+4,longword(j+8)+4);}
        }else if(kind==10){
            for(unsigned n=0;n<count;n++)palette[n]=0;
            j[0]=j[1]=0;
        }else{
            for(unsigned n=0;n<count;n++){
                uint16_t desired=0;
                if(kind==7)desired=word(target);
                else if(kind==8){
                    unsigned w=word(target+n*2),grey=(((w&15)+(w>>4&15)+(w>>8&15))/3)&65534;
                    desired=grey|grey<<4|grey<<8;
                }else if(kind==9)desired=word(target+n*2);
                if(kind==3 || kind==5){
                    unsigned value=0;
                    for(unsigned shift=0;shift<=8;shift+=4){
                        unsigned c=palette[n]>>shift&15;
                        if(kind==3){if(c)c=(c-2)&15;}else if(c!=14)c=(c+2)&15;
                        value|=c<<shift;
                    }
                    palette[n]=value;
                }else palette[n]=toward(palette[n],desired);
            }
            j[12]++;
            if(j[12]>=8){if(kind==3 || kind==5 || kind==7)j[0]=0;else j[0]=j[1]=0;}
        }
    }
    return true;
}
bool resource_palette_enqueue(resource_palette_state *s,const void *data,size_t size,uint32_t base,uint32_t command){
    if(!data)return false;
    single_source input={data,size,base};return resource_palette_enqueue_resolved(s,single,&input,command);
}
bool resource_palette_tick(resource_palette_state *s,const void *data,size_t size,uint32_t base,bool freeze){
    if(!data)return false;
    single_source input={data,size,base};return resource_palette_tick_resolved(s,single,&input,freeze);
}
