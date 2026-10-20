#include "resource_room.h"
#include <string.h>
#include "resource_checksum.h"

#define HEADER_BYTES 28u
typedef struct {uint8_t *out;const uint8_t *in;size_t at,cap;bool failed;uint8_t *const *blocks;size_t block_bytes;} codec;
static void raw(codec *c,void *p,size_t n){
    if(c->failed || c->at>c->cap || n>c->cap-c->at){c->failed=true;return;}
    if(c->in)memcpy(p,c->in+c->at,n);
    else if(c->out)memcpy(c->out+c->at,p,n);
    else if(c->blocks){
        size_t done=0;
        while(done<n){
            size_t at=c->at+done,offset=at%c->block_bytes;
            size_t count=c->block_bytes-offset;if(count>n-done)count=n-done;
            memcpy(c->blocks[at/c->block_bytes]+offset,(uint8_t *)p+done,count);done+=count;
        }
    }
    c->at+=n;
}
static uint32_t get32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
static void u16(codec *c,uint16_t *v){
    uint8_t p[2]={(uint8_t)*v,(uint8_t)(*v>>8)};raw(c,p,2);
    if(c->in)*v=(uint16_t)p[0]|(uint16_t)p[1]<<8;
}
static void u32(codec *c,uint32_t *v){uint8_t p[4];put32(p,*v);raw(c,p,4);if(c->in)*v=get32(p);}
static void flag(codec *c,bool *v){uint8_t b=*v;raw(c,&b,1);if(c->in){if(b>1)c->failed=true;*v=b!=0;}}
static void words(codec *c,uint16_t *v,unsigned count){for(unsigned i=0;i<count;i++)u16(c,v+i);}
static void longs(codec *c,uint32_t *v,unsigned count){for(unsigned i=0;i<count;i++)u32(c,v+i);}
static uint32_t checksum(const uint8_t *p,size_t n){
    return resource_crc32(p,n);
}
static void state(codec *c,resource_room *r,bool provenance,bool contact,bool flashlight,bool picture){
    resource_opening_state *v=&r->video;resource_scene_state *s=&v->scene;
#define RAW(field) raw(c,&(field),sizeof(field))
#define WORD(field) u16(c,&(field))
#define LONG(field) do {uint32_t value=(field);u32(c,&value);if(c->in)(field)=value;} while(0)
    RAW(s->globals);RAW(s->actors);
    RAW(s->palette.jobs);words(c,s->palette.colors,128);
    RAW(s->palette.cursor);RAW(s->palette.changed);RAW(s->palette.error);
    RAW(s->graphics.jobs);RAW(s->graphics.scratch);LONG(s->graphics.bank);RAW(s->graphics.error);
    RAW(s->scroll.control);RAW(s->scroll.shared);RAW(s->scroll.output);RAW(s->scroll.changed);
    RAW(s->vram);RAW(s->shared_ready);RAW(s->error);LONG(s->transfers);WORD(s->display_mode);
    longs(c,s->written_tiles,64);
    RAW(v->cards.main);RAW(v->cards.cached_registers);RAW(v->cards.vscroll);RAW(v->cards.hscroll);
    RAW(v->front_vram);RAW(v->pending_sat);RAW(v->front_sat);words(c,v->front_palette,64);
    words(c,(uint16_t *)v->raster_vertical,448);RAW(v->raster_visible);
    LONG(v->frames);LONG(v->published_transfers);LONG(v->clear_count);LONG(v->published_clears);LONG(v->vdp_command);
    words(c,v->live_vertical,2);LONG(v->pending_sprites);LONG(v->front_sprites);LONG(v->scene_id);
    RAW(v->visible);RAW(v->finished);RAW(v->error);RAW(v->raster_active);
    RAW(v->plane_request);RAW(v->hardware_flags);WORD(v->plane_b);WORD(v->cd_request);WORD(v->cd_status);
    words(c,v->pending_palette,64);flag(c,&v->delayed_palette);
    RAW(v->main_horizontal);words(c,(uint16_t *)v->raster_horizontal,448);
    resource_opening_presentation *p=&r->presentation;
    words(c,p->colors,64);words(c,p->from,64);words(c,p->target,64);words(c,p->elapsed,64);
    words(c,p->duration,64);words(c,p->fade_span,64);words(c,p->prior_span,64);LONG(p->scene);flag(c,&p->initialized);
    RAW(r->tasks.commands);WORD(r->tasks.index);WORD(r->tasks.counter);WORD(r->tasks.phase);WORD(r->tasks.timeout);
    RAW(r->tasks.active);RAW(r->tasks.caption_reset);RAW(r->tasks.error);
    WORD(r->devices.frame_clock);RAW(r->devices.pcm_flags);RAW(r->devices.music);RAW(r->devices.effect);
    LONG(r->scene);LONG(r->error);LONG(r->queued);RAW(r->queue);
    if(provenance){
        RAW(v->pending_owners);RAW(v->front_owners);
        longs(c,v->pending_maps,64);longs(c,v->front_maps,64);
        words(c,v->pending_portrait_palette,64);words(c,v->front_portrait_palette,64);
        flag(c,&v->pending_provenance_valid);flag(c,&v->front_provenance_valid);
    }
    if(contact){
        RAW(r->contact.bytes);WORD(r->current_pcm);WORD(r->menu_mode);
        RAW(r->pcm_suppression);flag(c,&r->contact_valid);
    }
    if(flashlight)RAW(r->flashlight);
    if(picture)WORD(s->picture_mode);
#undef RAW
#undef WORD
#undef LONG
}
size_t resource_room_checkpoint_bytes(resource_room *r){
    if(!r)return 0;
    codec c={.at=HEADER_BYTES,.cap=(size_t)-1};state(&c,r,true,true,true,true);return c.failed?0:c.at;
}
bool resource_room_checkpoint_export(resource_room *r,unsigned file,bool popup,void *output,size_t bytes){
    if(!r || !output || file>255 || r->scene>255 || r->error || r->video.error || r->tasks.error ||
       bytes!=resource_room_checkpoint_bytes(r) || bytes>256*1024)return false;
    uint8_t *p=output;memcpy(p,"NRCS",4);put32(p+4,7);put32(p+8,bytes);
    put32(p+12,file|(r->scene<<8)|((unsigned)popup<<16));
    put32(p+16,checksum(r->video.scene.resource,r->video.scene.resource_bytes));
    put32(p+20,checksum(r->video.scene.subcode,r->video.scene.subcode_bytes));put32(p+24,(uint32_t)r->combat_delay<<16);
    codec c={.out=p,.at=HEADER_BYTES,.cap=bytes};state(&c,r,true,true,true,true);return !c.failed && c.at==bytes;
}
bool resource_room_checkpoint_export_blocks(resource_room *r,unsigned file,bool popup,
    unsigned pcm,uint8_t *const blocks[],size_t block_bytes,size_t bytes){
    if(!r || !blocks || !block_bytes || file>255 || r->scene>255 || pcm>65535 ||
       r->error || r->video.error || r->tasks.error ||
       bytes!=resource_room_checkpoint_bytes(r) || bytes>256*1024)return false;
    for(size_t at=0;at<bytes;at+=block_bytes)if(!blocks[at/block_bytes])return false;
    uint8_t header[HEADER_BYTES];memcpy(header,"NRCS",4);put32(header+4,7);put32(header+8,bytes);
    put32(header+12,file|(r->scene<<8)|((unsigned)popup<<16));
    put32(header+16,checksum(r->video.scene.resource,r->video.scene.resource_bytes));
    put32(header+20,checksum(r->video.scene.subcode,r->video.scene.subcode_bytes));put32(header+24,pcm|((uint32_t)r->combat_delay<<16));
    codec c={.cap=bytes,.blocks=blocks,.block_bytes=block_bytes};raw(&c,header,sizeof(header));
    state(&c,r,true,true,true,true);return !c.failed && c.at==bytes;
}
bool resource_room_checkpoint_header(const void *input,size_t bytes,unsigned *file,unsigned *scene,bool *popup){
    if(!input || bytes<HEADER_BYTES || bytes>256*1024)return false;
    const uint8_t *p=input;uint32_t key=get32(p+12);
    unsigned version=get32(p+4),pcm=get32(p+24);
    if(memcmp(p,"NRCS",4) || version<1 || version>7 || get32(p+8)!=bytes || key>0x1ffff ||
       (version==1?pcm!=0:version<5 && pcm>(version>=4?65535u:255u)))return false;
    if(file)*file=key&255;
    if(scene)*scene=(key>>8)&255;
    if(popup)*popup=(key>>16)!=0;
    return true;
}
bool resource_room_checkpoint_with_pcm(void *input,size_t bytes,unsigned bank){
    if(!resource_room_checkpoint_header(input,bytes,NULL,NULL,NULL) ||
       bank>(get32((const uint8_t *)input+4)>=4?65535u:255u))return false;
    uint8_t *p=input;if(get32(p+4)<3)put32(p+4,2);put32(p+24,bank|(get32(p+4)>=5?(get32(p+24)&0xffff0000u):0));return true;
}
unsigned resource_room_checkpoint_pcm(const void *input,size_t bytes){
    return resource_room_checkpoint_header(input,bytes,NULL,NULL,NULL)?get32((const uint8_t *)input+24)&65535u:0;
}
bool resource_room_checkpoint_import(resource_room *r,const void *input,size_t bytes){
    unsigned file,scene;bool popup;
    if(!r || !resource_room_checkpoint_header(input,bytes,&file,&scene,&popup))return false;
    const uint8_t *p=input;bool provenance=get32(p+4)>=3,contact=get32(p+4)>=4,flashlight=get32(p+4)>=6,picture=get32(p+4)>=7;
    codec measure={.at=HEADER_BYTES,.cap=(size_t)-1};state(&measure,r,provenance,contact,flashlight,picture);
    if(measure.failed || bytes!=measure.at)return false;
    (void)file;(void)popup;
    if(get32(p+16)!=checksum(r->video.scene.resource,r->video.scene.resource_bytes) ||
       get32(p+20)!=checksum(r->video.scene.subcode,r->video.scene.subcode_bytes))return false;
    codec c={.in=p,.at=HEADER_BYTES,.cap=bytes};state(&c,r,provenance,contact,flashlight,picture);
    if(!picture){
        /* Old checkpoints retained SUB79EE and B6406 in the same word.
         * Saved room/menu contexts used32x64; a pending request did not exist. */
        r->video.scene.picture_mode=r->video.scene.display_mode&15;
        r->video.scene.display_mode=0;
        r->video.cards.cached_registers[4]=0x90;r->video.cards.cached_registers[5]=0x10;
    }
    r->combat_delay=get32(p+4)>=5?get32(p+24)>>16:0;
    if(!flashlight)memset(r->flashlight,0,sizeof(r->flashlight));
    if(!contact){
        memset(&r->contact,0,sizeof(r->contact));r->contact_valid=false;
        r->current_pcm=get32(p+24)?get32(p+24):65535;r->menu_mode=r->pcm_suppression=0;
    }
    if(!provenance){
        /* Old saves never stored the preceding actor/map provenance. Do
         * not guess it from the next pose. The next normal source ticks
         * publish it; menu saves do not expose voice portraits meanwhile. */
        memset(r->video.pending_owners,0,sizeof(r->video.pending_owners));
        memset(r->video.front_owners,0,sizeof(r->video.front_owners));
        memset(r->video.pending_maps,0,sizeof(r->video.pending_maps));
        memset(r->video.front_maps,0,sizeof(r->video.front_maps));
        memcpy(r->video.pending_portrait_palette,r->video.scene.palette.colors+64,128);
        memcpy(r->video.front_portrait_palette,r->video.scene.palette.colors+64,128);
        r->video.pending_provenance_valid=r->video.front_provenance_valid=false;
    }
    return !c.failed && c.at==bytes && !r->error && !r->video.error && !r->video.scene.error &&
           !r->video.scene.palette.error && !r->video.scene.graphics.error && !r->tasks.error &&
           r->menu_mode<=1 && resource_flashlight_valid(r->flashlight) &&
           r->flashlight[9]<36 && r->flashlight[10]<20 && !r->flashlight[12] && r->flashlight[13]<=10 &&
           r->scene==scene && r->scene<=255 && r->queued<=510 && !(r->queued&1) &&
           r->tasks.index<=510 && !(r->tasks.index&1) && r->tasks.active<=1 && r->tasks.phase<=3 &&
           r->video.pending_sprites<=64 && r->video.front_sprites<=64 && r->video.scene.palette.cursor<12 &&
           r->video.plane_b<=0xf000 && !(r->video.plane_b&0x7ff);
}
