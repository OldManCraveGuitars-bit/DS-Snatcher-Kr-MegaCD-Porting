#include "resource_opening.h"
#include "resource_main_scroll.h"
#include <string.h>
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
#include <nds.h>
volatile uint32_t resource_opening_tick_profile[32][10];
static unsigned tick_profile_index;
static uint32_t tick_profile_begin,tick_profile_publish,tick_profile_actors;
static bool tick_profile_active;
#endif
static uint16_t word(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static void putword(uint8_t *p,uint16_t v){p[0]=v>>8;p[1]=v;}
static void putlong(uint8_t *p,uint32_t v){putword(p,v>>16);putword(p+2,v);}
static uint8_t *g(resource_opening_state *s,unsigned at){return s->scene.globals+at-0x7860;}
static bool job(void *ctx,unsigned kind,unsigned bytes,uint32_t at,uint32_t value){
    resource_opening_state *s=ctx;
    switch(kind){
    case 1:return resource_scene_clear(&s->scene);
    case 2:s->scene_id=value;return resource_scene_load(&s->scene,value);
    case 3:return resource_palette_enqueue_resolved(&s->scene.palette,resource_scene_resolve,&s->scene,value);
    case 4:*g(s,0x78c7)=1;resource_scroll_mode(&s->scene.scroll,value);return true;
    case 5:
        if(at==0x7846 && bytes==2){if(value!=1)return false;s->finished=1;return true;}
        if(at==0xb642d && bytes==1){s->plane_request[1]=value;return true;}
        if(at>=0x7a00 && at+bytes<=0x8e00 && (bytes==1 || bytes==2 || bytes==4)){
            uint8_t *p=s->scene.actors+at-0x7a00;
            if(bytes==1)*p=value;else if(bytes==2)putword(p,value);else putlong(p,value);
            return true;
        }
        if(at>=0x8f00 && at+bytes<=0x8f40 && (bytes==1 || bytes==2 || bytes==4)){
            uint8_t *p=s->scene.scroll.control+at-0x8f00;
            if(bytes==1)*p=value;else if(bytes==2)putword(p,value);else putlong(p,value);
            return true;
        }
        if(at<0x7860 || at+bytes>0x78e0 || (bytes!=1 && bytes!=2 && bytes!=4))return false;
        if(bytes==1)*g(s,at)=value;else if(bytes==2)putword(g(s,at),value);else putlong(g(s,at),value);
        return true;
    case 6:
        if(at!=0xff800f || bytes!=1 || value!=16)return false;
        s->hardware_flags|=value;return true;
    case 7:
        if(at || bytes || (value!=4 && value!=5))return false;
        s->cd_request=value;return true;
    default:return false;
    }
}
static bool port(void *ctx,uint32_t at,unsigned size,uint32_t value){
    resource_opening_state *s=ctx;
    if(at==0xc00004){
        if(size==4){s->vdp_command=value;return true;}
        if(size!=2)return false;
        unsigned reg=(value>>8)&31;
        if((value&0xc000)==0x8000){if(reg==1)s->visible=(value&64)!=0;return true;}
        return false;
    }
    if(at==0xc00000 && s->vdp_command==0x40000010){
        if(size==2){s->live_vertical[0]=value;return true;}
        if(size==4){s->live_vertical[0]=value>>16;s->live_vertical[1]=value;return true;}
    }
    if(at==0xc00000 && size==4 && s->vdp_command==0x78000003){
        /* Mode15's HBlank restores both horizontal bases below its window.
         * Preserve preceding lines instead of shifting the whole picture. */
        unsigned first=word(s->cards.main+0x34)*8;
        for(unsigned y=first;y<224;y++){
            s->raster_horizontal[0][y]=(int16_t)(value>>16);
            s->raster_horizontal[1][y]=(int16_t)value;
        }
        return true;
    }
    return false;
}
static void secondary_clear(resource_opening_state *s){
    memset(s->cards.main+0x70,0,8);putword(s->cards.cached_registers+6,word(s->cards.cached_registers+6)&0xfff8);
    memset(s->cards.vscroll,0,4);memset(s->cards.hscroll,0,4);
}
static bool main_modes(resource_opening_state *s){
    uint8_t *c=s->cards.main,*shared=s->scene.scroll.shared;
    /* FF3246..FF326C: low byteFF selects VDP8407 (planeB E000),
     * other nonzero requests restore8406 (C000), retaining the native ack. */
    if(s->plane_request[1]){
        if(s->plane_request[1]==255){s->plane_b=0xe000;s->plane_request[0]=255;}
        else{s->plane_b=0xc000;memset(s->plane_request,0,2);}
    }
    if(shared[0]){
        unsigned request=shared[0];shared[0]=0;
        if(!resource_cards_primary(&s->cards,request,port,s))return false;
    }
    if(shared[4]){
        unsigned request=shared[4];shared[4]=0;
        if(request==252 || request==255){
            putword(c+0x78,0);memset(c+0x86,0,6);memset(c+0x60,0,16);secondary_clear(s);
        }else if(request>=1 && request<=6){
            c[0x70]=request;
            if(c[0x71]!=request){
                c[0x71]=request;
                if(request<3){putword(c+0x76,0);putword(c+0x72,0);putword(c+0x74,0);}
                else{putword(c+0x78,0);putword(c+0x86,0);}
            }
        }else return false;
    }
    /* Main FF345A runs the active secondary effect before a new request.
     * Modes3..6 own original backgroundB line-scroll bands and pulses. */
    unsigned active=word(c+0x78);
    if(active>=3 && active<=6 && !resource_main_scroll_tick(&s->cards,s->main_horizontal,s->frames,port,s))return false;
    if(c[0x70]>=3 && c[0x70]<=6 && !resource_main_scroll_tick(&s->cards,s->main_horizontal,s->frames,port,s))return false;
    if(c[0x70]==1 || c[0x70]==2){
        putword(s->cards.cached_registers+6,word(s->cards.cached_registers+6)&0xfff8);
        putword(c+0x76,1);
    }
    if(c[0x60] && !resource_cards_tick(&s->cards,port,s))return false;
    /* FF3CAA: EB00 is VSRAM, E700 is horizontal scroll. The original card
     * helper clears both and disables secondary scroll before its bands. */
    if(word(c+0x76)){
        putword(s->cards.hscroll,(-word(s->scene.scroll.output+2))&1023);
        putword(s->cards.hscroll+2,(-word(s->scene.scroll.output+6))&1023);
        putword(s->cards.vscroll,word(s->scene.scroll.output)&1023);
        putword(s->cards.vscroll+2,word(s->scene.scroll.output+4)&1023);
    }
    if((word(s->cards.cached_registers+6)&3)==3){
        unsigned count=word(c+0x7a)+1;if(count>216)return false;
        /* FF3D5E uploads backgroundB only, starting at native scanline8.
         * Rows beyond the last updated band retain their previous value. */
        for(unsigned row=0;row<count;row++)s->raster_horizontal[1][row+8]=(int16_t)word(s->main_horizontal+0x22+row*4);
    }else{
        for(unsigned plane=0;plane<2;plane++)s->raster_horizontal[plane][0]=(int16_t)word(s->cards.vscroll+plane*2);
    }
    return true;
}
static bool raster(resource_opening_state *s){
    unsigned effect=word(s->cards.main+0x6c);
    bool contact=effect==9 || (effect>=17 && effect<=20);
    s->raster_active=(effect>=2 && effect<=6) || contact;
    s->visible=1;s->live_vertical[0]=word(s->cards.hscroll);s->live_vertical[1]=word(s->cards.hscroll+2);
    if(effect==9 && (word(s->cards.cached_registers+6)&3)!=3)
        for(unsigned p=0;p<2;p++)for(unsigned y=0;y<224;y++)
            s->raster_horizontal[p][y]=(int16_t)word(s->cards.vscroll+p*2);
    for(unsigned y=0;y<224;y++){
        /* The original HBlank counter0 is in effect at visible scanline0.
         * The native8A07 bands therefore change at0,8,16,..., not7,15,... . */
        if(s->raster_active && !(y&7)){
            if(contact)putword(s->cards.main+0x34,y>>3);
            if(!resource_cards_hblank(&s->cards,y>>3,port,s))return false;
            if(contact)putword(s->cards.main+0x34,(y>>3)+1);
        }
        s->raster_vertical[0][y]=(int16_t)s->live_vertical[0];s->raster_vertical[1][y]=(int16_t)s->live_vertical[1];s->raster_visible[y]=s->visible;
    }
    return true;
}
bool resource_opening_bind_scene(resource_opening_state *s,const void *resource,size_t bytes,const void *sub,size_t subbytes,uint32_t rng){
    if(!s || !resource)return false;
    memset(s,0,sizeof(*s));
    if(!resource_scene_bind(&s->scene,resource,bytes,sub,subbytes))return false;
    putlong(g(s,0x786a),rng);putword(g(s,0x78b2),1);
    putword(s->cards.cached_registers,0x8004);putword(s->cards.cached_registers+2,0x8164);putword(s->cards.cached_registers+6,0x8b00);
    s->plane_b=0xc000;
    return resource_background_bind_vram(s->front_vram,s->front_palette,32,64,0xe000,0xc000,0,0,256,224);
}
bool resource_opening_bind(resource_opening_state *s,const void *bank,size_t bytes,const void *sub,size_t subbytes,uint32_t rng){
    resource_callback_state callbacks;
    if(!s || !resource_callback_bind(&callbacks,bank,bytes) || !resource_opening_bind_scene(s,
       callbacks.bank+callbacks.resource_offset,callbacks.resource_size,sub,subbytes,rng))return false;
    s->callbacks=callbacks;putword(g(s,0x7896),s->callbacks.index);return true;
}
bool resource_opening_publish(resource_opening_state *s){
    if(!s || !s->scene.resource)return false;
    memset(s->portrait_written_tiles,0,sizeof(s->portrait_written_tiles));
    memset(s->display_changed_tiles,0,sizeof(s->display_changed_tiles));
    s->portrait_publication++;
    /* Publish the previous SUB update, then execute this original VBlank's
     * commands. Converted pictures are never an input to either side. */
    if(s->published_transfers!=s->scene.transfers || s->published_clears!=s->clear_count){
        if(!resource_background_publish_vram(s->front_vram,s->scene.vram,s->scene.written_tiles,
            s->display_changed_tiles,s->deferred_display))return false;
        memcpy(s->portrait_written_tiles,s->scene.written_tiles,sizeof(s->portrait_written_tiles));
        memset(s->scene.written_tiles,0,sizeof(s->scene.written_tiles));
        s->published_transfers=s->scene.transfers;s->published_clears=s->clear_count;
    }
    if(s->delayed_palette){
        memcpy(s->front_palette,s->pending_palette,sizeof(s->front_palette));
        memcpy(s->pending_palette,s->scene.palette.colors,sizeof(s->pending_palette));
    }else memcpy(s->front_palette,s->scene.palette.colors,sizeof(s->front_palette));
    /* Original lower portrait palette is the second64-colour work bank.
     * Preserve the same publication latency as the upper palette. */
    if(s->delayed_palette){
        memcpy(s->front_portrait_palette,s->pending_portrait_palette,sizeof(s->front_portrait_palette));
        memcpy(s->pending_portrait_palette,s->scene.palette.colors+64,sizeof(s->pending_portrait_palette));
    }else memcpy(s->front_portrait_palette,s->scene.palette.colors+64,sizeof(s->front_portrait_palette));
    s->scene.palette.changed=0;
    memcpy(s->front_sat,s->pending_sat,512);s->front_sprites=s->pending_sprites;
    memcpy(s->front_owners,s->pending_owners,64);
    memcpy(s->front_maps,s->pending_maps,sizeof(s->front_maps));
    s->front_provenance_valid=s->pending_provenance_valid;
    if(!main_modes(s) || !raster(s)){s->error=1;return false;}
    return true;
}
bool resource_opening_tick_cd(resource_opening_state *s,uint32_t clock,uint16_t iterations,uint16_t cd_status){
    if(!s || !s->callbacks.bank)return false;
    s->error=0;s->cd_status=cd_status;
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    tick_profile_active=true;tick_profile_begin=cpuGetTiming();
#endif
    if(!resource_opening_publish(s))return false;
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    tick_profile_publish=cpuGetTiming();
#endif
    uint32_t rng=longword(g(s,0x786a));resource_actor_random(&rng);putlong(g(s,0x786a),rng);
    putlong(g(s,0x787a),clock);putword(g(s,0x78b2),iterations);
    if(!s->finished && !*g(s,0x7887) && !*g(s,0x7886)){
        int callback=s->callbacks.index<0?
            resource_callback_initialize(&s->callbacks,word(g(s,0x7898)),cd_status,job,s):
            resource_callback_tick_cd(&s->callbacks,clock,cd_status,job,s);
        if(callback<0){s->error=2;return false;}
        putword(g(s,0x7896),s->callbacks.index);
        if(!*g(s,0x7887) && !*g(s,0x7886) && !resource_scene_actors_tick(&s->scene,0)){s->error=3;return false;}
    }
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    tick_profile_actors=cpuGetTiming();
#endif
    return resource_opening_finish_tick(s);
}
bool resource_opening_finish_tick(resource_opening_state *s){
    if(!s || !s->scene.resource)return false;
    resource_scroll_tick(&s->scene.scroll,0);
    uint16_t offsets[4];for(unsigned i=0;i<4;i++)offsets[i]=word(s->scene.scroll.output+i*2);
    if(!resource_sprites_build_owned(s->scene.actors,resource_scene_resolve,&s->scene,s->frames,offsets,s->pending_sat,&s->pending_sprites,s->pending_owners,s->pending_maps)){s->error=4;return false;}
    s->pending_provenance_valid=true;
    bool frozen=*g(s,0x7886)!=0;
    if(!frozen)*g(s,0x78ca)=0;
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t sprites_end=cpuGetTiming();
#endif
    if(!resource_palette_tick_resolved(&s->scene.palette,resource_scene_resolve,&s->scene,frozen)){s->error=5;return false;}
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t palette_end=cpuGetTiming();
#endif
    if(!resource_graphics_tick(&s->scene.graphics,resource_scene_resolve,resource_scene_dma,&s->scene,frozen,false,0)){s->error=5;return false;}
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t graphics_end=cpuGetTiming();
#endif
    if(!resource_scene_tiles_tick(&s->scene,false)){s->error=5;return false;}
    if(s->scene.shared_ready){
        /* Original Main FF16BC/FF16C8 clear both plane tables before loads. */
        memset(s->scene.vram+0xc000,0,16384);s->scene.shared_ready=0;s->clear_count++;
        for(unsigned i=48;i<64;i++)s->scene.written_tiles[i]=~0u;
    }
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    if(tick_profile_active){
        uint32_t end=cpuGetTiming();
        volatile uint32_t *p=resource_opening_tick_profile[tick_profile_index++&31];
        p[0]=s->frames;p[1]=s->scene_id;p[2]=tick_profile_publish-tick_profile_begin;
        p[3]=tick_profile_actors-tick_profile_publish;p[4]=sprites_end-tick_profile_actors;
        p[5]=palette_end-sprites_end;p[6]=graphics_end-palette_end;p[7]=end-graphics_end;
        p[8]=end-tick_profile_begin;p[9]=end;tick_profile_active=false;
    }
#endif
    s->frames++;return true;
}
bool resource_opening_tick(resource_opening_state *s,uint32_t clock,uint16_t iterations){
    return resource_opening_tick_cd(s,clock,iterations,0);
}
bool resource_opening_draw_view(resource_opening_state *s,uint16_t out[256*192],const uint16_t rgb[64],unsigned y,unsigned h){
    return resource_opening_draw_area(s,out,rgb,0,y,256,h);
}
static bool prepare_draw_area(resource_opening_state *s,const uint16_t rgb[64],unsigned x,unsigned y,unsigned w,unsigned h){
    if(!s || !s->scene.resource)return false;
    if(!resource_background_planes(0xe000,s->plane_b))return false;
    if(!resource_background_view(x,y,w,h))return false;
    if(rgb){if(!resource_background_palette_rgb555(rgb))return false;}
    else if(!resource_background_palette(0,s->front_palette,64))return false;
    for(unsigned i=0;i<s->front_sprites;i++){
        const uint8_t *p=s->front_sat+i*8;s->sprites[i].x=(word(p+6)&511)-128;s->sprites[i].y=(word(p)&511)-128;
        s->sprites[i].size=word(p+2);s->sprites[i].attribute=word(p+4);
    }
    for(unsigned p=0;p<2;p++)if(!resource_background_scroll(p,word(s->cards.vscroll+p*2),-word(s->cards.hscroll+p*2)))return false;
    if(!resource_background_raster(s->raster_vertical,s->raster_visible) ||
       !resource_background_line_scroll(((word(s->cards.cached_registers+6)&3)==3 || word(s->cards.main+0x6c)==9)?s->raster_horizontal:NULL) ||
       !resource_background_sprites(s->sprites,s->front_sprites))return false;
    return true;
}
bool resource_opening_draw_area(resource_opening_state *s,uint16_t out[256*192],const uint16_t rgb[64],unsigned x,unsigned y,unsigned w,unsigned h){
    return out && prepare_draw_area(s,rgb,x,y,w,h) && resource_background_draw(out);
}
bool resource_opening_draw_view_indexed_rgb555(resource_opening_state *s,uint16_t out[256*192],const uint16_t rgb[64],unsigned y,unsigned h){
    return resource_opening_draw_view_indexed_rgb555_cached(s,out,rgb,y,h,NULL,NULL);
}
bool resource_opening_draw_view_indexed_rgb555_cached(resource_opening_state *s,uint16_t out[256*192],const uint16_t rgb[64],unsigned y,unsigned h,
    resource_background_picture *picture,bool *copied){
    if(copied)*copied=false;
    return out && prepare_draw_area(s,rgb,0,y,256,h) && resource_background_draw_indexed_rgb555_cached(out,picture,copied);
}
bool resource_opening_draw_area8(resource_opening_state *s,uint8_t out[256*192],unsigned x,unsigned y,unsigned w,unsigned h){
    uint16_t indices[64];for(unsigned i=0;i<64;i++)indices[i]=0x8000|i;
    return out && prepare_draw_area(s,indices,x,y,w,h) && resource_background_draw_indexed8(out,32,224);
}
bool resource_opening_draw_view8(resource_opening_state *s,uint8_t out[256*192],unsigned y,unsigned h){
    uint16_t indices[64];for(unsigned i=0;i<64;i++)indices[i]=0x8000|i;
    return out && prepare_draw_area(s,indices,0,y,256,h) && resource_background_draw_indexed8_opaque(out);
}
bool resource_opening_draw(resource_opening_state *s,uint16_t out[256*192]){
    return resource_opening_draw_view(s,out,NULL,0,224);
}


bool resource_opening_draw_area8_retained(resource_opening_state *s,uint8_t out[256*192],unsigned x,unsigned y,unsigned w,unsigned h,resource_background_picture *picture){
    uint16_t indices[64];for(unsigned i=0;i<64;i++)indices[i]=0x8000|i;
    return out && prepare_draw_area(s,indices,x,y,w,h) && resource_background_draw_area8_retained(out,picture);
}


bool resource_opening_draw_room_indices_cached(resource_opening_state *s,uint8_t out[256*192],const uint16_t rgb[64],
    resource_background_picture *picture,bool *copied){
    return out && prepare_draw_area(s,rgb,0,8,256,192) &&
        resource_background_draw_indexed8_area_cached(out,picture,copied,32,224,128);
}

bool resource_opening_draw_full_indices_cached(resource_opening_state *s,uint8_t out[256*192],const uint16_t rgb[64],resource_background_picture *picture,bool *copied){
 return out && prepare_draw_area(s,rgb,0,0,256,224) && resource_background_draw_indexed8_area_cached(out,picture,copied,0,256,192);
}

bool resource_opening_draw_native_rows_retained(resource_opening_state *s,uint8_t out[256*192],unsigned y,unsigned rows,resource_background_picture *picture){
    uint16_t indices[64];for(unsigned i=0;i<64;i++)indices[i]=0x8000|i;
    /* A192-high viewport maps row n to source y+n. Only the actual native
     * picture height is composed, so the unused tail is never sampled. */
    return out && rows && rows<=192 && prepare_draw_area(s,indices,0,y,256,192) &&
        resource_background_draw_native_rows_retained(out,rows,picture);
}

bool resource_opening_draw_native_packed(resource_opening_state *s,uint8_t *out,unsigned y,unsigned rows){
    uint16_t indices[64];for(unsigned i=0;i<64;i++)indices[i]=0x8000|i;
    return out && rows && rows<=192 && prepare_draw_area(s,indices,0,y,256,192) &&
        resource_background_draw_native_packed(out,rows);
}

bool resource_opening_draw_native_packed_retained(resource_opening_state *s,uint8_t *out,unsigned y,unsigned rows,
    const uint8_t *previous,resource_background_picture *picture){
    uint16_t indices[64];for(unsigned i=0;i<64;i++)indices[i]=0x8000|i;
    return out && rows && rows<=192 && prepare_draw_area(s,indices,0,y,256,192) &&
        resource_background_draw_native_packed_retained(out,rows,previous,picture);
}

bool resource_opening_draw_terminal_indices(resource_opening_state *s,uint8_t out[256*192],
    const uint16_t rgb[64],resource_background_picture *picture,bool *copied){
    return out && prepare_draw_area(s,rgb,0,8,256,192) &&
        resource_background_draw_indexed8_area_cached(out,picture,copied,0,256,144);
}

bool resource_opening_draw_practice_indices(resource_opening_state *s,uint8_t out[256*192],
    const uint16_t rgb[64],resource_background_picture *picture,bool *copied){
    return out && prepare_draw_area(s,rgb,0,8,256,192) &&
        resource_background_draw_indexed8_area_cached(out,picture,copied,16,240,128);
}

bool resource_opening_draw_area_compact(resource_opening_state *s,uint16_t out[256*192],const uint16_t rgb[64],unsigned x,unsigned y,unsigned w,unsigned h){
    if(!out || !rgb || !prepare_draw_area(s,rgb,x,y,w,h) ||
       !resource_background_draw_indexed8((uint8_t*)out,0,256))return false;
    /* Expand backwards: output words cannot overwrite unread index bytes.
     * Retain all source planes, sprites, priority and the same fitted view. */
    uint8_t *indices=(uint8_t*)out;
    for(unsigned i=256*192;i--;)out[i]=rgb[indices[i]&63]|0x8000;
    return true;
}
