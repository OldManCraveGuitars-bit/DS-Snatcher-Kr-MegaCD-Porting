#include "resource_scene.h"
#include "resource_lzkn1.h"
#include <string.h>
static uint16_t word(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static void putword(uint8_t *p,uint16_t v){p[0]=v>>8;p[1]=v;}
static void putlong(uint8_t *p,uint32_t v){putword(p,v>>16);putword(p+2,v);}
static uint8_t *g(resource_scene_state *s,unsigned at){return s->globals+at-0x7860;}
const uint8_t *resource_scene_resolve(void *ctx,uint32_t at,size_t *remaining){
    resource_scene_state *s=ctx;*remaining=0;
    if(at>=0x28000 && at-0x28000<s->resource_bytes){*remaining=s->resource_bytes-(at-0x28000);return s->resource+at-0x28000;}
    if(at>=0xd400 && at-0xd400<s->subcode_bytes){*remaining=s->subcode_bytes-(at-0xd400);return s->subcode+at-0xd400;}
    return NULL;
}
static const uint8_t *read(resource_scene_state *s,uint32_t at,size_t bytes){
    size_t remaining=0;const uint8_t *p=resource_scene_resolve(s,at,&remaining);return p && remaining>=bytes?p:NULL;
}
bool resource_scene_bind(resource_scene_state *s,const void *resource,size_t size,const void *subcode,size_t subsize){
    if(!s || !resource || size<32 || !subcode || subsize<0xfcb4-0xd400)return false;
    memset(s,0,sizeof(*s));s->resource=resource;s->resource_bytes=size;s->subcode=subcode;s->subcode_bytes=subsize;
    resource_palette_reset(&s->palette);return read(s,longword(s->resource+4),2)!=NULL;
}
bool resource_scene_blackout(resource_scene_state *s){
    if(!s || !s->resource)return false;
        *g(s,0x78c7)=1;
        if(!(*g(s,0x788a)&1)){
            *g(s,0x788a)|=1;memset(s->palette.jobs,0,sizeof(s->palette.jobs));
            unsigned first=16;
            if((*g(s,0x788a)&2) || *g(s,0x7866)){*g(s,0x788a)&=~2;first=0;}
            memset(s->palette.colors+first,0,(64-first)*sizeof(uint16_t));
            if(!(*g(s,0x788a)&4)){memset(s->palette.colors+80,0,48*sizeof(uint16_t));s->palette.changed|=2;}
            *g(s,0x788a)&=~4;s->palette.changed|=1;*g(s,0x78ca)=1;
        }
    return true;
}
bool resource_scene_shared_palette(resource_scene_state *s){
    if(!s || !s->resource)return false;
    for(unsigned i=0;i<8;i++){
        const uint8_t *p=read(s,0xf600+i*8,8);
        if(!p || word(p)!=0x33fc || longword(p+4)!=0xb6100+i*2)return false;
        s->palette.colors[i]=word(p+2);
    }
    s->palette.changed|=1;return true;
}
bool resource_scene_clear(resource_scene_state *s){
    if(!s || !s->resource)return false;
    /* F560 is skipped when the scene requests palette retention (788B). */
    if(!*g(s,0x788b) && !resource_scene_blackout(s))return false;
    memset(s->palette.jobs,0,sizeof(s->palette.jobs));
    if(*g(s,0x7866)){
        *g(s,0x7866)=0;
        if(!resource_scene_shared_palette(s))return false;
    }
    resource_graphics_clear(&s->graphics);resource_scroll_mode(&s->scroll,252);*g(s,0x78c7)=1;
    for(unsigned id=1;id<64;id++)if(!(s->actors[id*80+0x4e]&1) && word(s->actors+id*80))memset(s->actors+id*80,0,80);
    if(!(*g(s,0x788b)&2))*g(s,0x7887)=1;
    *g(s,0x788b)=0;return true;
}
static bool load_table(resource_scene_state *s,uint32_t table,unsigned scene){
    if(!s || !s->resource)return false;
    const uint8_t *entry=read(s,table+scene*2,2);if(!entry)return false;
    uint32_t at=table+word(entry);
    for(unsigned budget=0;budget<256;budget++){
        const uint8_t *packet=read(s,at,2);if(!packet)return false;
        unsigned op=packet[0],n=packet[1];if(op>=128)return true;
        const uint8_t *p=read(s,at+2,n);if(!p)return false;
        if(op==0){if(!resource_palette_enqueue_resolved(&s->palette,resource_scene_resolve,s,at+2))return false;}
        else if(op==1){
            if(n<2 || (n-2)%8 || word(p+n-2)!=65535)return false;
            *g(s,0x788a)&=~1;*g(s,0x78bd)=*g(s,0x78bc);putword(g(s,0x78be),0);
            putlong(g(s,0x7880),at+2);*g(s,0x7886)=1;
        }else if(op==2){
            if(!n || n%12)return false;
            for(unsigned i=0;i<n;i+=12){
                unsigned id=p[i];if(id>=64)return false;uint8_t *a=s->actors+id*80;
                if(a[0])return true; /* Native nonzero high type byte stops this packet. */
                memset(a,0,80);putword(a,1);a[0x25]=p[i+1];putword(a+6,word(p+i+2));putword(a+10,word(p+i+4));
                uint32_t maps=longword(p+i+6),sequence=at+2+i+12+(int16_t)word(p+i+10);
                putlong(a+0x2e,maps);putlong(a+0x32,sequence);putword(a+0x28,65535);
                const uint8_t *c=read(s,sequence,8);if(!c)return false;unsigned pose=c[0];
                if(pose==0xa1 || pose==0xa7){a[0x16]=c[1];pose=c[4];}
                if(pose<128){const uint8_t *m=read(s,maps+pose*2,2);if(!m)return false;putlong(a+0x36,maps+word(m));}
            }
        }else if(op==3 || op==4){
            if(!n || n%2)return false;
            for(unsigned i=0;i<n;i+=2){
                unsigned parent=p[i],child=p[i+1];if(parent>=64 || child>=64)return false;
                uint8_t *a=s->actors+parent*80,*b=s->actors+child*80;
                putword(b+0x4c,word(a+0x4a));putword(a+0x4a,child);putword(b+0x48,parent);
            }
        }else if(op>=5 && op<=7){if(n<2)return false;*g(s,0x7893)=p[1];}
        else if(op==8){if(n<2)return false;s->display_mode=word(p);}
        else if(op==9){if(n<2)return false;*g(s,0x78bc)=p[1];}
        else return false;
        at+=2+n;
    }
    return false;
}
bool resource_scene_load(resource_scene_state *s,unsigned scene){
    return s && s->resource && load_table(s,longword(s->resource+4),scene);
}
bool resource_scene_popup(resource_scene_state *s,unsigned picture,unsigned position){
    /* Original1449A calls F696's SUBCODE scene table, then moves actor1
     * using its original position table. The arguments are picture/position,
     * not DATA_H file/scene numbers. Existing room actors remain alive. */
    if(!s || !s->resource || position>=18)return false;
    const uint8_t *count=read(s,0x146ce,2),*xy=read(s,0x144c6+position*4,4);
    if(!count || picture>=word(count)/2 || position>=18 || !xy || !load_table(s,0x146ce,picture))return false;
    putword(s->actors+80+6,word(xy));putword(s->actors+80+10,word(xy+2));return true;
}
bool resource_scene_dma(void *ctx,uint32_t source,uint16_t destination,const uint8_t *data,unsigned bytes){
    (void)source;resource_scene_state *s=ctx;
    if(!data || bytes>sizeof(s->vram)-destination)return false;
    memcpy(s->vram+destination,data,bytes);
    if(bytes)for(unsigned tile=destination/32;tile<=(destination+bytes-1)/32;tile++)
        s->written_tiles[tile>>5]|=1u<<(tile&31);
    s->transfers++;return true;
}
bool resource_scene_tiles_tick(resource_scene_state *s,bool busy){
    if(!s || !s->resource)return false;
    uint8_t *ready=g(s,0x7887);
    if(*ready){if(*ready==1){s->shared_ready=3;*ready=2;}else *ready=0;return true;}
    if(!*g(s,0x7886))return true;
    unsigned delay=word(g(s,0x7884));if(delay){putword(g(s,0x7884),delay-1);return true;}
    if(busy)return true;
    uint32_t at=longword(g(s,0x7880));const uint8_t *p=read(s,at,2);if(!p)return false;
    if(word(p)==65535){*g(s,0x7886)=0;return true;}
    p=read(s,at,8);if(!p)return false;putword(g(s,0x7884),word(p+4));
    uint32_t source=longword(p)&0xffffff;unsigned destination=word(p+6),offset=0,n;
    if(!*g(s,0x78bd) || !word(g(s,0x78be))){
        size_t left=0,written=0,consumed=0;const uint8_t *compressed=resource_scene_resolve(s,source,&left);
        if(!compressed || !resource_lzkn1(compressed,left,s->graphics.scratch,8192,&written,&consumed))return false;
        n=written;if(*g(s,0x78bd))putword(g(s,0x78c0),n);
    }else n=word(g(s,0x78c0));
    if(*g(s,0x78bd)){
        offset=word(g(s,0x78be));
        if(n>2048){putword(g(s,0x78c0),n-2048);n=2048;putword(g(s,0x78be),offset+2048);}
        else{putlong(g(s,0x7880),at+8);putword(g(s,0x78be),0);}
    }else putlong(g(s,0x7880),at+8);
    if(offset>8192 || n>8192-offset || destination+offset>65535)return false;
    return resource_scene_dma(s,0x238000+offset,destination+offset,s->graphics.scratch+offset,n);
}
bool resource_scene_actor_effect(void *ctx,unsigned actor,unsigned op,unsigned arg,uint16_t value,uint32_t sequence){
    resource_scene_state *s=ctx;uint32_t at=sequence+(int16_t)value;
    switch(op){
    case 0x88:
        if(*g(s,0x78ca))return true;
        if(!resource_palette_enqueue_resolved(&s->palette,resource_scene_resolve,s,at))return false;
        *g(s,0x78c7)=1;return true;
    case 0x89:memset(s->palette.jobs,0,sizeof(s->palette.jobs));return true;
    case 0x8a:return resource_graphics_enqueue(&s->graphics,resource_scene_resolve,s,at);
    case 0x8c:case 0x8d:case 0x8e:case 0x8f:*g(s,0x78c7)=1;return resource_scroll_actor(&s->scroll,op,arg,value);
    case 0x9b:*g(s,0x788b)=arg;return true;
    case 0x9c:if(actor>=64)return false;*g(s,0x788b)=s->actors[actor*80+0x4e];return true;
    case 0xa2:if(actor>=64)return false;s->actors[actor*80+0x4f]=*g(s,0x7886)?1:0;return true;
    case 0xa3:*g(s,0x7866)=1;return true;
    case 0xa4:*g(s,0x788a)|=1;return true;
    case 0xa5:return resource_graphics_enqueue(&s->graphics,resource_scene_resolve,s,0xfc84);
    case 0xa6:return resource_graphics_enqueue(&s->graphics,resource_scene_resolve,s,0xfca2);
    case 0xa8:*g(s,0x78c7)=1;resource_scroll_mode(&s->scroll,arg);return true;
    case 0xab:*g(s,0x7891)=arg;return true;
    case 0xae:*g(s,0x788a)|=2;return true;
    default:return false; /* Sound exports require their original driver. */
    }
}
typedef struct {
    resource_scene_state *scene;
    resource_scene_devices *devices;
    resource_actor_clock *clock;
} actor_context;
static const uint8_t *actor_resolve(void *ctx,uint32_t at,size_t bytes){return read(((actor_context *)ctx)->scene,at,bytes);}
static bool actor_effect(void *ctx,unsigned actor,unsigned op,unsigned arg,uint16_t value,uint32_t sequence){
    actor_context *v=ctx;resource_scene_state *s=v->scene;resource_scene_devices *d=v->devices;
    uint32_t entry=0;unsigned channel=0;
    switch(op){
    case 0xaa:
        if(!d)return false;
        putword(g(s,0x78aa),arg?d->frame_clock-word(g(s,0x78aa)):d->frame_clock);return true;
    case 0x9d:
        if(*g(s,0x78c3))return true;
        if(!d || !d->sound)return false;
        d->effect=arg==0x3d?0:arg;entry=0x150fa;break;
    case 0x9f:
        if(*g(s,0x7890) || (v->clock->pcm_flags&7))return true;
        if(!d || !d->sound)return false;
        entry=0x150ee;channel=255;break;
    case 0xad:
        if(!d || !d->sound)return false;
        d->music=arg==255?0:arg;entry=0x150fa;channel=1;break;
    default:return resource_scene_actor_effect(s,actor,op,arg,value,sequence);
    }
    if(!d->sound(d->context,entry,arg,channel,d))return false;
    v->clock->pcm_flags=d->pcm_flags;return true;
}
static bool actors_tick(resource_scene_state *s,uint8_t pcm_flags,resource_scene_devices *devices){
    if(!s || !s->resource)return false;
    resource_actor_clock clock={0};clock.rng=longword(g(s,0x786a));clock.pcm_clock=longword(g(s,0x787a));
    clock.iterations=word(g(s,0x78b2));clock.freeze=*g(s,0x78c6);clock.pcm_flags=pcm_flags;
    actor_context context={s,devices,&clock};
    resource_actor_effects effects={&context,actor_effect,*g(s,0x78ca)!=0,actor_resolve};uint16_t iterations;
    if(!resource_actor_tick_registers(s->actors,s->resource,s->resource_bytes,0x28000,&clock,UINT64_MAX,&effects,&iterations)){
        s->error=clock.error;return false;
    }
    putlong(g(s,0x786a),clock.rng);putword(g(s,0x7868),iterations);return true;
}
bool resource_scene_actors_tick(resource_scene_state *s,uint8_t pcm_flags){return actors_tick(s,pcm_flags,NULL);}
bool resource_scene_actors_tick_devices(resource_scene_state *s,resource_scene_devices *devices){
    if(!devices)return false;
    return actors_tick(s,devices->pcm_flags,devices);
}
