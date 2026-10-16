#include "resource_live_portrait.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
typedef struct {uint32_t map;int x0,y0,x1,y1;uint64_t sprites;} group;
bool resource_live_portraits_find(const uint8_t sat[512],unsigned count,
    const uint8_t owners[64],const uint32_t maps[64],
    resource_live_portrait portraits[3],unsigned *found){
    if(!sat || count>64 || !owners || !maps || !portraits || !found)return false;
    *found=0;
    /* At most64 original actors/maps. Scratch is caller-synchronous and
     * static to avoid adding kilobytes to the small DS user stack. */
    static group groups[64];unsigned n=0;
    for(unsigned i=0;i<count;i++){
        if(owners[i]>=64)return false;
        /* Original text actor0 and item/cursor helpers1..4 are never faces. */
        if(owners[i]<5 || !maps[i])continue;
        const uint8_t *s=sat+i*8;
        int x=(int)(word(s+6)&511)-128,y=(int)(word(s)&511)-128;
        unsigned size=word(s+2),w=((size>>10)&3)*8+8,h=((size>>8)&3)*8+8;
        /* Source portrait sprites occupy y149..212. Do not infer a face
         * from a crop of the main illustration, a popup or arbitrary ink. */
        if(x<0 || x+(int)w>256 || y<149 || y+(int)h>213)continue;
        unsigned j=0;while(j<n && groups[j].map!=maps[i])j++;
        if(j==n){groups[n++]=(group){maps[i],INT_MAX,INT_MAX,INT_MIN,INT_MIN,0};}
        group *g=groups+j;
        if(x<g->x0)g->x0=x;
        if(y<g->y0)g->y0=y;
        if(x+(int)w>g->x1)g->x1=x+(int)w;
        if(y+(int)h>g->y1)g->y1=y+(int)h;
        g->sprites|=UINT64_C(1)<<i;
    }
    for(unsigned j=0;j<n;j++){
        const group *g=groups+j;
        if(g->x1-g->x0!=64 || g->y0!=149 || g->y1!=213)continue;
        if(*found==3)return false;
        unsigned k=(*found)++;
        while(k && portraits[k-1].x>g->x0){portraits[k]=portraits[k-1];k--;}
        portraits[k]=(resource_live_portrait){g->map,(int16_t)g->x0,149,g->sprites};
    }
    return true;
}
static uint16_t color(unsigned v){
    unsigned r=v&15,g=v>>4&15,b=v>>8&15;
    /* The old 4->5/6->8->5 conversion is exactly nibble replication for
     * all 4096 source RGB combinations. Keep its integer result directly. */
    r=r*2|(r>>3);g=g*2|(g>>3);b=b*2|(b>>3);
    return 0x8000|r|(g<<5)|(b<<10);
}
bool resource_live_portrait_draw(const resource_live_portrait *p,
    const uint8_t sat[512],unsigned count,const uint8_t vram[65536],
    const uint16_t palette[64],uint16_t pixels[4096]){
    if(!p || !sat || count>64 || !vram || !palette || !pixels ||
       (count<64 && p->sprites>>count) || !p->sprites)return false;
    uint16_t rgb[64];for(unsigned i=0;i<64;i++)rgb[i]=color(palette[i]);
    for(unsigned i=0;i<4096;i++)pixels[i]=0x8000;
    /* The earliest native SAT entry wins overlaps, including transparent
     * tiles. Read column-major4bpp patterns directly from published VRAM. */
    for(unsigned i=count;i--;){
        if(!(p->sprites&(UINT64_C(1)<<i)))continue;
        const uint8_t *s=sat+i*8;unsigned attr=word(s+4),size=word(s+2);
        unsigned w=((size>>10)&3)*8+8,h=((size>>8)&3)*8+8;
        int sx=(int)(word(s+6)&511)-128-p->x,sy=(int)(word(s)&511)-128-p->y;
        for(unsigned y=0;y<h;y++){
            int dy=sy+(int)y;if(dy<0 || dy>=64)continue;
            unsigned ty=attr&4096?h-1-y:y;
            for(unsigned x=0;x<w;x++){
                int dx=sx+(int)x;if(dx<0 || dx>=64)continue;
                unsigned tx=attr&2048?w-1-x:x;
                unsigned tile=((attr&2047)+(tx/8)*(h/8)+ty/8)&2047;
                unsigned packed=vram[tile*32+(ty&7)*4+(tx&7)/2];
                unsigned ink=tx&1?packed&15:packed>>4;
                if(ink)pixels[dy*64+dx]=rgb[((attr>>13)&3)*16+ink];
            }
        }
    }
    return true;
}

bool resource_live_portrait_draw_cached(resource_live_portrait_cache *cache,
    const resource_live_portrait *p,const uint8_t sat[512],unsigned count,
    const uint8_t vram[65536],const uint16_t palette[64],
    uint32_t transfers,uint32_t clears,uint16_t pixels[4096],bool *changed){
    if(changed)*changed=false;
    if(!cache || !p || !sat || count>64 || !vram || !palette || !pixels ||
       !p->sprites || (count<64 && p->sprites>>count))return false;
    /* Compare exact selected records; unrelated background/text records
     * and SAT link numbering cannot invalidate a stationary face. */
    static uint8_t records[512];unsigned selected=0;
    for(unsigned i=0;i<count;i++)if(p->sprites&(UINT64_C(1)<<i)){
        memcpy(records+selected*8,sat+i*8,8);
        records[selected*8+3]=0;selected++;
    }
    if(cache->valid && cache->count==selected && cache->map==p->map &&
       cache->x==p->x && cache->y==p->y && cache->transfers==transfers && cache->clears==clears &&
       !memcmp(cache->palette,palette,128) && !memcmp(cache->records,records,selected*8))return true;
    if(!resource_live_portrait_draw(p,sat,count,vram,palette,pixels))return false;
    cache->valid=true;cache->count=selected;cache->map=p->map;cache->x=p->x;cache->y=p->y;
    cache->transfers=transfers;cache->clears=clears;
    memcpy(cache->palette,palette,128);memcpy(cache->records,records,selected*8);
    if(changed)*changed=true;
    return true;
}

bool resource_live_portrait_draw_indices(const resource_live_portrait *p,
    const uint8_t sat[512],unsigned count,const uint8_t vram[65536],
    uint8_t pixels[4096]){
    if(!p || !sat || count>64 || !vram || !pixels ||
       (count<64 && p->sprites>>count) || !p->sprites)return false;
    memset(pixels,0,4096);
    /* The earliest native SAT entry wins overlaps, including transparent
     * tiles. Read column-major4bpp patterns directly from published VRAM. */
    for(unsigned i=count;i--;){
        if(!(p->sprites&(UINT64_C(1)<<i)))continue;
        const uint8_t *s=sat+i*8;unsigned attr=word(s+4),size=word(s+2);
        unsigned w=((size>>10)&3)*8+8,h=((size>>8)&3)*8+8;
        int sx=(int)(word(s+6)&511)-128-p->x,sy=(int)(word(s)&511)-128-p->y;
        unsigned columns=w/8,rows=h/8,palette=((attr>>13)&3)*16;
        for(unsigned y=0;y<h;y++){
            int dy=sy+(int)y;if(dy<0 || dy>=64)continue;
            unsigned ty=attr&4096?h-1-y:y;
            for(unsigned column=0;column<columns;column++){
                int dx=sx+(int)column*8;
                if(dx>=64 || dx+8<=0)continue;
                unsigned source_column=attr&2048?columns-1-column:column;
                unsigned tile=((attr&2047)+source_column*rows+ty/8)&2047;
                const uint8_t *pattern=vram+tile*32+(ty&7)*4;
                unsigned first=dx<0?(unsigned)-dx:0;
                unsigned end=dx+8>64?(unsigned)(64-dx):8;
                for(unsigned x=first;x<end;x++){
                    unsigned tx=attr&2048?7-x:x;
                    unsigned packed=pattern[tx/2];
                    unsigned ink=tx&1?packed&15:packed>>4;
                    if(ink)pixels[dy*64+dx+(int)x]=palette+ink;
                }
            }
        }
    }
    return true;
}

void resource_live_portrait_palette(const uint16_t palette[64],uint16_t rgb[64]){
    for(unsigned i=0;i<64;i++)rgb[i]=color(palette[i]);
    /* Source ink0 is transparent; untouched portrait pixels are opaque black. */
    rgb[0]=0x8000;
}
