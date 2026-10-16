#include "resource_opening_presentation.h"
#include <string.h>
static uint16_t native_color(unsigned v){
    unsigned r=v&15,g=v>>4&15,b=v>>8&15;
    r=r*2|(r>>3);g=g*4|(g>>2);b=b*2|(b>>3);
    return 0x8000|((r*255/31)>>3)|((g*255/63)>>3)<<5|((b*255/31)>>3)<<10;
}
static uint32_t longword(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
void resource_opening_presentation_reset(resource_opening_presentation *p){if(p)memset(p,0,sizeof(*p));}
bool resource_opening_presentation_tick(resource_opening_presentation *p,const resource_opening_state *s){
    if(!p || !s)return false;
    uint16_t span[64]={0};
    for(unsigned j=0;j<12;j++){
        const uint8_t *job=s->scene.palette.jobs+j*16;
        unsigned kind=job[0],first=job[2]/2,count=job[3]+1;
        if(kind!=3 && kind!=5 && kind!=7 && kind!=8 && kind!=9)continue;
        uint32_t at=longword(job+8);
        size_t remaining=0;const uint8_t *command=resource_scene_resolve((void*)&s->scene,at,&remaining);
        if(!command || remaining<4)return false;
        unsigned delay=command[1]+1;
        for(unsigned i=first;i<first+count && i<64;i++)span[i]=delay;
    }
    if(!p->initialized || p->scene!=s->scene_id){
        resource_opening_presentation_reset(p);p->initialized=true;p->scene=s->scene_id;
        for(unsigned i=0;i<64;i++)p->colors[i]=p->from[i]=p->target[i]=native_color(s->front_palette[i]);
    }
    for(unsigned i=0;i<64;i++){
        uint16_t target=native_color(s->front_palette[i]);
        if(target!=p->target[i]){
            p->from[i]=p->colors[i];p->target[i]=target;p->elapsed[i]=0;
            /* The final original fade step clears its job in the same tick.
             * Retain its preceding interval for that last transition only. */
            p->duration[i]=span[i]?span[i]:p->fade_span[i]?p->fade_span[i]:p->prior_span[i];
            if(!p->duration[i])p->colors[i]=target;
        }
        unsigned n=p->duration[i];
        if(n && p->elapsed[i]<n){
            unsigned t=++p->elapsed[i],value=0x8000;
            for(unsigned shift=0;shift<15;shift+=5){
                unsigned a=p->from[i]>>shift&31,b=p->target[i]>>shift&31;
                value|=((a*(n-t)+b*t+n/2)/n)<<shift;
            }
            p->colors[i]=value;
        }
        p->prior_span[i]=p->fade_span[i];p->fade_span[i]=span[i];
    }
    return true;
}
static bool draw_presented(resource_opening_state *s,const resource_opening_presentation *p,uint16_t out[256*192],bool indexed){
    if(!s || !p || !p->initialized)return false;
    /* Original 8px/12px English letter rows must not be dropped by224->192.
     * These three DATA_D1 scenes are text/license cards, with all ink inside
     * native y16..207. Animated scene pictures retain the full224-row view. */
    bool card=s->callbacks.track==3 && s->scene_id<=2;
    unsigned blank=0;
    if(card){
        /* The original text cards use a solid tile (e.g. index8), not just
         * VDP backdrop index0. Identify that tile from the original map.
         * Only its background colour is transparent; glyph ink and the
         * original line-by-line reveal remain in front of the UI layer. */
        unsigned attr=(unsigned)s->front_vram[0xe000]<<8|s->front_vram[0xe001];
        const uint8_t *tile=s->front_vram+(attr&2047)*32;
        unsigned nibble=tile[0]&15;bool uniform=tile[0]==nibble*17;
        for(unsigned i=1;i<32;i++)if(tile[i]!=tile[0])uniform=false;
        if(uniform)blank=(attr>>13&3)*16+nibble;
    }
    resource_background_ui_blank(blank);
    return indexed?resource_opening_draw_view_indexed_rgb555(s,out,p->colors,card?16:0,card?192:224):
        resource_opening_draw_view(s,out,p->colors,card?16:0,card?192:224);
}
bool resource_opening_draw_presented(resource_opening_state *s,const resource_opening_presentation *p,uint16_t out[256*192]){
    return draw_presented(s,p,out,false);
}
bool resource_opening_draw_presented_indexed_rgb555(resource_opening_state *s,const resource_opening_presentation *p,uint16_t out[256*192]){
    return draw_presented(s,p,out,true);
}
bool resource_opening_draw_presented8(resource_opening_state *s,const resource_opening_presentation *p,uint8_t out[256*192]){
    if(!s || !p || !p->initialized)return false;
    bool card=s->callbacks.track==3 && s->scene_id<=2;
    return resource_opening_draw_view8(s,out,card?16:0,card?192:224);
}
