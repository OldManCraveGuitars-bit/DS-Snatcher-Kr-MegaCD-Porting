#include "portrait_live.h"
#include <string.h>
extern uint16_t portrait_images[3][4096];
#define images portrait_images
static resource_live_portrait_cache cache[3];
static int positions[3];
static unsigned count,source_epoch;
static uint64_t presented_identity[3];
static const resource_opening_state *source;
extern unsigned portrait_generation;

unsigned portrait_live_count(void){return count;}
int portrait_live_x(unsigned slot){return slot<count?positions[slot]:0;}
const uint16_t *portrait_live_image(unsigned slot){return slot<count?images[slot]:NULL;}
bool portrait_live_source(const resource_opening_state *v,unsigned epoch){
    memset(presented_identity,0,sizeof(presented_identity));
    if(source!=v || source_epoch!=epoch){memset(cache,0,sizeof(cache));source=v;source_epoch=epoch;}
    resource_live_portrait faces[3];unsigned n=0;
    if(v && v->front_provenance_valid && !resource_live_portraits_find(v->front_sat,v->front_sprites,
        v->front_owners,v->front_maps,faces,&n))return false;
    bool changed=count!=n;
    for(unsigned i=0;i<n;i++){
        bool drawn=false;
        if(i>=count)cache[i].valid=false;
        if(!resource_live_portrait_draw_cached(cache+i,faces+i,v->front_sat,v->front_sprites,
            v->front_vram,v->front_portrait_palette,v->published_transfers,v->published_clears,
            images[i],&drawn))return false;
        changed|=drawn || positions[i]!=faces[i].x;positions[i]=faces[i].x;
    }
    count=n;if(changed)portrait_generation++;return true;
}
bool portrait_live_snapshot(const resource_portrait_snapshot *s){
    if(!s || s->count>3)return false;
    source=NULL;memset(cache,0,sizeof(cache));
    bool changed=count!=s->count;
    for(unsigned i=0;i<s->count;i++){
        const resource_portrait_picture *p=s->pictures[i];
        if(!p || !p->references || p->x<0 || p->x>192)return false;
        if(i<count && positions[i]==p->x && presented_identity[i]==p->identity)continue;
        presented_identity[i]=p->identity;
        if(i>=count || positions[i]!=p->x || !resource_portrait_picture_equal(p,images[i])){
            resource_portrait_picture_unpack(p,images[i]);positions[i]=p->x;changed=true;
        }
    }
    count=s->count;if(changed)portrait_generation++;return true;
}
