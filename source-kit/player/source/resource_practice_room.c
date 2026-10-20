#include "resource_practice_room.h"
#include <string.h>
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static void putword(uint8_t *p,unsigned v){p[0]=v>>8;p[1]=v;}
static bool request(void *context,uint32_t entry,uint32_t argument,unsigned channel){
    resource_practice *p=context;resource_room *r=p->device_context;
    if(entry==0xe81a){
        if(argument==0x12ea8)return resource_graphics_enqueue_health(&r->video.scene.graphics,
            resource_scene_resolve,&r->video.scene,p->health_vram);
        return resource_graphics_enqueue(&r->video.scene.graphics,resource_scene_resolve,&r->video.scene,argument);
    }
    if(entry==0xe41a)return resource_palette_enqueue_resolved(&r->video.scene.palette,
        resource_scene_resolve,&r->video.scene,argument);
    if(entry==0xe4a0){
        /* E4A0 clears 48 longwords of jobs, not CRAM or the queue cursor. */
        memset(r->video.scene.palette.jobs,0,sizeof(r->video.scene.palette.jobs));return true;
    }
    return r->devices.sound && r->devices.sound(r->devices.context,entry,argument,channel,&r->devices);
}
bool resource_practice_room_begin(resource_practice *p,resource_room *r){
    return resource_native_combat_room_begin(p,r,0);
}
bool resource_native_combat_room_begin(resource_practice *p,resource_room *r,unsigned stage){
    if(!p || !r || r->error || (stage!=0 && stage!=2 && stage!=3 && stage!=4 && stage!=5 && stage!=6 && stage!=7 && stage!=8 && stage!=9 && stage!=10 && stage!=11 && stage!=12 && stage!=13 && stage!=14) ||
       r->scene!=(stage>=12 && stage<=14?stage-11:(stage==0 || stage==11 || stage==7 || stage==10)?0u:stage==8?5u:stage==6?3u:stage==2?7u:stage==4?1u:2u) || r->tasks.active || r->queued)return false;
    resource_scene_state *s=&r->video.scene;
    if(!resource_practice_bind(p,s->actors,s->resource,s->resource_bytes,s->subcode,s->subcode_bytes))return false;
    memcpy(p->ram+0x60,s->globals,128);p->context=p;p->device_context=r;p->request=request;
    /* Source174D2..174FA stage request, with native task24 represented by
     * this caller's active controller rather than a fabricated scene. */
    putword(p->ram+0xe0,stage);putword(p->ram+0xe8,0);putword(p->ram+0xea,0);
    putword(p->ram+0xe4,0);putword(p->ram+0xe6,0);p->ram[0xaf]=255;
    /* SUB1750E can set native78E2 before the controller is created.
     * Preserve that word through the original task24 initialization. */
    putword(p->ram+0xe2,r->combat_delay);r->combat_delay=0;
    memcpy(s->globals,p->ram+0x60,128);
    return true;
}
bool resource_practice_room_reset_begin(resource_practice *p,resource_room *r){
    if(!p || !r || r->error || r->scene!=0 || r->tasks.active || r->queued)return false;
    resource_scene_state *s=&r->video.scene;
    if(!resource_practice_bind(p,s->actors,s->resource,s->resource_bytes,s->subcode,s->subcode_bytes))return false;
    memcpy(p->ram+0x60,s->globals,128);p->context=p;p->device_context=r;p->request=request;return true;
}
static bool room_tick(resource_practice *p,resource_room *r,unsigned held,unsigned pressed,bool *reset_done){
    if(!p || !r || p->error || r->error || r->tasks.active || r->queued ||
       p->actors!=r->video.scene.actors || p->context!=p || p->device_context!=r)return false;
    resource_opening_state *v=&r->video;
    if(!resource_opening_publish(v)){r->error=1;return false;}
    uint8_t *g=v->scene.globals;
    uint32_t rng=(uint32_t)word(g+10)<<16|word(g+12);
    resource_actor_random(&rng);putword(g+10,rng>>16);putword(g+12,rng);
    putword(g+0x52,1);r->devices.frame_clock=v->frames;
    if(!g[0x27] && !g[0x26]){
        /* Source D6FE..D722: controller/task24 precedes the actor pass.
         * The DS display is ready to acknowledge the original two-tick
         * display handshake; the controller still enforces that delay. */
        memcpy(p->ram+0x60,g,128);p->ram[0x190]=held;p->ram[0x191]=pressed;
        p->host_ready=(p->host_request&32);p->pcm_flags=r->devices.pcm_flags;
        if(reset_done){
            putword(p->ram+0x48,v->frames);
            if(!resource_practice_score_reset(p,reset_done))return false;
        }else if(!resource_practice_call(p,0x1208e))return false;
        memcpy(g,p->ram+0x60,128);
        if(!resource_scene_actors_tick_devices(&v->scene,&r->devices)){r->error=4;return false;}
    }
    if(!resource_opening_finish_tick(v)){r->error=5;return false;}
    if(!resource_opening_presentation_tick(&r->presentation,v)){r->error=6;return false;}
    return true;
}
bool resource_practice_room_tick(resource_practice *p,resource_room *r,unsigned held,unsigned pressed){
    return room_tick(p,r,held,pressed,NULL);
}
bool resource_practice_room_reset_tick(resource_practice *p,resource_room *r,bool *complete){
    if(!complete)return false;
    *complete=false;return room_tick(p,r,0,0,complete);
}
bool resource_practice_room_done(const resource_practice *p){return p && !p->error && !p->ram[0xaf];}
