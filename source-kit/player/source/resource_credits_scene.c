#include "resource_credits_scene.h"
#include <string.h>
static uint32_t getlong(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void putword(uint8_t *p,uint16_t v){p[0]=v>>8;p[1]=v;}
static void putlong(uint8_t *p,uint32_t v){putword(p,v>>16);putword(p+2,v);}
static bool job(void *ctx,unsigned kind,unsigned bytes,uint32_t at,uint32_t value){
    resource_credits_scene *s=ctx;
    if(bytes || at)return false;
    switch(kind){
    case 1:return resource_scene_clear(&s->video.scene);
    case 2:s->video.scene_id=value;return resource_scene_load(&s->video.scene,value);
    case 3:return resource_palette_enqueue_resolved(&s->video.scene.palette,resource_scene_resolve,&s->video.scene,value);
    case 7:if(value!=2 && value!=19)return false;s->video.cd_request=value;return true;
    default:return false;
    }
}
bool resource_credits_scene_begin(resource_credits_scene *s,const void *data,size_t bytes,
    const void *sub,size_t subbytes,uint32_t rng){
    if(!s)return false;
    memset(s,0,sizeof(*s));
    if(!resource_credits_bind(&s->control,data,bytes) ||
       !resource_opening_bind_scene(&s->video,data,bytes,sub,subbytes,rng))return false;
    putword(s->video.scene.globals+0x36,65535);
    return true;
}
bool resource_credits_scene_tick(resource_credits_scene *s,uint32_t clock,uint16_t status,
    uint8_t special,uint8_t input){
    if(!s || !s->control.data)return false;
    resource_opening_state *v=&s->video;
    if(!resource_opening_publish(v))return false;
    uint8_t *g=v->scene.globals;
    uint32_t rng=getlong(g+0x0a);resource_actor_random(&rng);putlong(g+0x0a,rng);
    putlong(g+0x1a,clock);putword(g+0x52,1);v->cd_status=status;
    if(!v->finished && !g[0x27] && !g[0x26]){
        if(resource_credits_tick(&s->control,clock,status,special,input,job,s)<0){v->error=2;return false;}
        putlong(g+0x1a,s->control.clock);putword(g+0x36,s->control.index);putword(g+0x38,s->control.phase);
        v->finished=s->control.finished!=0;
        if(!g[0x27] && !g[0x26] && !resource_scene_actors_tick(&v->scene,0)){v->error=3;return false;}
    }
    return resource_opening_finish_tick(v) && resource_opening_presentation_tick(&s->presentation,v);
}
