#include "resource_portrait_history.h"
#include <stdlib.h>
#include <string.h>
/* The source repeats mouth/eye poses non-consecutively. All delayed frames
 * still exist, but equal resolved pixels share one immutable allocation.
 * A hash only narrows the search; full pixels and position must match.
 * The pool owns no references, so the final frame release frees its picture. */
static resource_portrait_picture *pool;
static uint64_t picture_identity;
typedef struct resource_portrait_pattern {
    struct resource_portrait_pattern *next,*previous;
    unsigned references,bytes;
    uint32_t hash;
    uint8_t data[];
} resource_portrait_pattern;
static resource_portrait_pattern *pattern_pool;
volatile unsigned resource_portrait_patterns,resource_portrait_pattern_peak;
static resource_portrait_pattern *pattern_take(const void *data,unsigned bytes){
    const uint8_t *b=data;uint32_t hash=2166136261u;
    for(unsigned i=0;i<bytes;i++)hash=(hash^b[i])*16777619u;
    for(resource_portrait_pattern *p=pattern_pool;p;p=p->next)
        if(p->bytes==bytes && p->hash==hash && !memcmp(p->data,data,bytes)){p->references++;return p;}
    resource_portrait_pattern *p=malloc(sizeof(*p)+bytes);if(!p)return NULL;
    p->references=1;p->bytes=bytes;p->hash=hash;memcpy(p->data,data,bytes);
    p->previous=NULL;p->next=pattern_pool;if(pattern_pool)pattern_pool->previous=p;pattern_pool=p;
    resource_portrait_patterns++;
    if(resource_portrait_patterns>resource_portrait_pattern_peak)resource_portrait_pattern_peak=resource_portrait_patterns;
    return p;
}
static void pattern_release(resource_portrait_pattern *p){
    if(!p || --p->references)return;
    if(p->previous)p->previous->next=p->next;else pattern_pool=p->next;
    if(p->next)p->next->previous=p->previous;
    resource_portrait_patterns--;free(p);
}
volatile unsigned resource_portrait_unique,resource_portrait_peak,resource_portrait_reuses;
static uint32_t picture_hash(const uint16_t pixels[4096]){
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<4096;i+=2)
        hash=(hash^(pixels[i]|(uint32_t)pixels[i+1]<<16))*16777619u;
    return hash;
}
/* Exact RGB555+opaque-bit values, including original black. A raw fallback
 * keeps arbitrary >128-colour input lossless too. Static scratch is safe:
 * snapshots run in the non-reentrant manual audio fill, never the IRQ. */
static uint32_t color_keys[256];
static uint8_t color_ids[256],packed_indices[4096];
static uint16_t packed_colors[128];
volatile unsigned resource_portrait_snapshot_fault;
bool resource_portrait_picture_equal(const resource_portrait_picture *p,const uint16_t pixels[4096]){
    if(!p || !pixels)return false;
    if(!p->colors)return !memcmp(p->pattern->data,pixels,8192);
    const uint8_t *indices=p->pattern->data;
    for(unsigned i=0;i<4096;i++)if(p->data[indices[i]]!=pixels[i])return false;
    return true;
}
void resource_portrait_picture_unpack(const resource_portrait_picture *p,uint16_t pixels[4096]){
    if(!p->colors){memcpy(pixels,p->pattern->data,8192);return;}
    const uint8_t *indices=p->pattern->data;
    for(unsigned i=0;i<4096;i++)pixels[i]=p->data[indices[i]];
}
static __attribute__((unused)) resource_portrait_picture *intern_picture(int x,const uint16_t pixels[4096]){
    uint32_t hash=picture_hash(pixels);
    for(resource_portrait_picture *p=pool;p;p=p->next)
        if(p->x==x && p->fingerprint==hash && resource_portrait_picture_equal(p,pixels)){
            resource_portrait_reuses++;return p;
        }
    unsigned colors=0;memset(color_keys,0,sizeof(color_keys));
    for(unsigned i=0;i<4096;i++){
        uint32_t key=(uint32_t)pixels[i]+1;
        unsigned at=(key^(key>>7)^(key>>12))&255;
        while(color_keys[at] && color_keys[at]!=key)at=(at+1)&255;
        if(!color_keys[at]){
            if(colors==128){colors=0;break;}
            color_keys[at]=key;color_ids[at]=colors;packed_colors[colors++]=pixels[i];
        }
        packed_indices[i]=color_ids[at];
    }
    resource_portrait_pattern *pattern=pattern_take(colors?(const void*)packed_indices:pixels,colors?4096:8192);
    if(!pattern){resource_portrait_snapshot_fault=4;return NULL;}
    resource_portrait_picture *p=malloc(sizeof(*p)+colors*2);
    if(!p){pattern_release(pattern);resource_portrait_snapshot_fault=4;return NULL;}
    p->source_palette=0;p->references=0;p->x=x;p->fingerprint=hash;p->colors=colors;p->pattern=pattern;
    p->identity=++picture_identity;
    if(colors)memcpy(p->data,packed_colors,colors*2);
    p->previous=NULL;p->next=pool;if(pool)pool->previous=p;pool=p;
    resource_portrait_unique++;
    if(resource_portrait_peak<resource_portrait_unique)resource_portrait_peak=resource_portrait_unique;
    return p;
}

/* A native pattern has stable VDP palette indices, including during an
 * all-black fade where RGB alone cannot recover which source colour it was. */
static resource_portrait_picture *intern_native_picture(int x,resource_portrait_pattern *pattern,
    const uint16_t colors[64]){
    for(resource_portrait_picture *p=pool;p;p=p->next)
        if(p->source_palette && p->x==x && p->pattern==pattern && !memcmp(p->data,colors,128)){
            pattern_release(pattern);resource_portrait_reuses++;return p;
        }
    resource_portrait_picture *p=malloc(sizeof(*p)+128);
    if(!p){pattern_release(pattern);resource_portrait_snapshot_fault=4;return NULL;}
    p->references=0;p->x=x;p->fingerprint=0;p->colors=64;p->source_palette=1;
    p->pattern=pattern;p->identity=++picture_identity;memcpy(p->data,colors,128);
    p->previous=NULL;p->next=pool;if(pool)pool->previous=p;pool=p;
    resource_portrait_unique++;
    if(resource_portrait_peak<resource_portrait_unique)resource_portrait_peak=resource_portrait_unique;
    return p;
}
static bool native_layout_update(resource_live_portrait_cache *cache,const resource_live_portrait *p,
    const resource_opening_state *v){
    static uint8_t records[512];unsigned selected=0;
    for(unsigned i=0;i<v->front_sprites;i++)if(p->sprites&(UINT64_C(1)<<i)){
        memcpy(records+selected*8,v->front_sat+i*8,8);records[selected*8+3]=0;selected++;
    }
    bool same=cache->valid && cache->count==selected && cache->map==p->map &&
        cache->x==p->x && cache->y==p->y && cache->transfers==v->published_transfers &&
        cache->clears==v->published_clears && !memcmp(cache->records,records,selected*8);
    cache->valid=true;cache->count=selected;cache->map=p->map;cache->x=p->x;cache->y=p->y;
    cache->transfers=v->published_transfers;cache->clears=v->published_clears;
    memcpy(cache->records,records,selected*8);memcpy(cache->palette,v->front_portrait_palette,128);
    return same;
}
volatile unsigned resource_portrait_native_recolours,resource_portrait_native_decodes;
static uint16_t source_colors[64];

void resource_portrait_snapshot_release(resource_portrait_snapshot *s){
    if(!s)return;
    for(unsigned i=0;i<s->count;i++){
        resource_portrait_picture *p=s->pictures[i];
        if(p && !--p->references){
            if(p->previous)p->previous->next=p->next;else pool=p->next;
            if(p->next)p->next->previous=p->previous;
            resource_portrait_unique--;pattern_release(p->pattern);free(p);
        }
    }
    memset(s,0,sizeof(*s));
}
volatile unsigned resource_portrait_unrelated_writes;
static bool face_tiles_untouched(const resource_opening_state *v,const resource_live_portrait *p){
    for(unsigned i=0;i<v->front_sprites;i++)if(p->sprites&(UINT64_C(1)<<i)){
        const uint8_t *s=v->front_sat+i*8;
        unsigned size=(unsigned)s[2]<<8|s[3],attr=(unsigned)s[4]<<8|s[5];
        unsigned count=((size>>10&3)+1)*((size>>8&3)+1);
        for(unsigned j=0;j<count;j++){
            unsigned tile=((attr&2047)+j)&2047;
            if(v->portrait_written_tiles[tile>>5]&(1u<<(tile&31)))return false;
        }
    }
    return true;
}
bool resource_portrait_snapshot_take(resource_portrait_history *h,
    resource_portrait_snapshot *out,const resource_portrait_snapshot *previous,
    const resource_opening_state *v,unsigned bank){
    if(!h || !out || out==previous || !v)return false;
    resource_portrait_snapshot_release(out);
    if(!h->initialized || h->bank!=bank){
        memset(h->cache,0,sizeof(h->cache));h->initialized=true;h->bank=bank;
    }
    if(!v->front_provenance_valid)return true;
    resource_live_portrait faces[3];unsigned count=0;
    if(!resource_live_portraits_find(v->front_sat,v->front_sprites,v->front_owners,v->front_maps,faces,&count))return false;
    /* Every face in this publication uses the same original palette. */
    if(count)resource_live_portrait_palette(v->front_portrait_palette,source_colors);
    for(unsigned i=0;i<count;i++){
        resource_portrait_picture *prior=previous && i<previous->count?previous->pictures[i]:NULL;
        if(!prior)h->cache[i].valid=false;
        /* Only consecutive publications have a complete dirty set. Skipped
         * samples, resets and clears use the existing conservative redraw.
         * Palette and exact SAT records are still checked by the decoder. */
        resource_live_portrait_cache *cache=h->cache+i;
        if(cache->valid && h->publication && v->portrait_publication==h->publication+1 &&
           cache->clears==v->published_clears && cache->transfers!=v->published_transfers &&
           face_tiles_untouched(v,faces+i)){
            cache->transfers=v->published_transfers;resource_portrait_unrelated_writes++;
        }
        bool same_layout=native_layout_update(cache,faces+i,v);
        if(prior && prior->source_palette && prior->x==faces[i].x && same_layout &&
           !memcmp(prior->data,source_colors,128)){
            out->pictures[i]=prior;
        }else{
            resource_portrait_pattern *pattern;
            if(prior && prior->source_palette && same_layout){
                pattern=prior->pattern;pattern->references++;resource_portrait_native_recolours++;
            }else{
                if(!resource_live_portrait_draw_indices(faces+i,v->front_sat,v->front_sprites,
                    v->front_vram,(uint8_t*)h->scratch))goto fail;
                pattern=pattern_take(h->scratch,4096);resource_portrait_native_decodes++;
                if(!pattern){resource_portrait_snapshot_fault=4;goto fail;}
            }
            resource_portrait_picture *p=intern_native_picture(faces[i].x,pattern,source_colors);
            if(!p)goto fail;
            out->pictures[i]=p;
        }
        out->pictures[i]->references++;out->count++;
    }
    h->publication=v->portrait_publication;
    return true;
fail:
    h->publication=0;
    resource_portrait_snapshot_release(out);memset(h->cache,0,sizeof(h->cache));return false;
}

