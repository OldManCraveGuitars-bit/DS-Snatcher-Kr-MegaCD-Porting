#include "resource_contact.h"
#include <string.h>
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static void putword(uint8_t *p,unsigned n){p[0]=n>>8;p[1]=n;}
bool resource_contact_capture(resource_contact_state *s,const resource_contact_context *c,
    const uint8_t actors[64*80]){
    if(!s || !c || !actors)return false;
    putword(s->bytes,c->scene);putword(s->bytes+2,c->pcm);s->bytes[4]=c->music;
    s->bytes[5]=(c->display_mode&15)|((c->menu_mode&3)<<4)|
        (c->pcm_suppression?64:0)|(c->actor_pcm_suppression?128:0);
    /* Source leaves the two reserved bytes untouched. */
    for(unsigned i=0;i<64;i++){
        const uint8_t *a=actors+i*80;unsigned flags=0;
        if(word(a)){
            flags=128;
            if(a[0x4e]&128)flags|=64;
            else flags|=(a[0x25]&3)|(a[0x24]&12)|((a[0x4e]&1)?16:0)|(a[0x26]?32:0);
        }
        s->bytes[8+i]=flags;
    }
    return true;
}
size_t resource_contact_return(const resource_contact_state *s,resource_contact_context *c,
    uint8_t *queue,size_t capacity){
    if(!s || !c || !queue)return 0;
    uint8_t packet[26];size_t n=0;unsigned scene=word(s->bytes),pcm=word(s->bytes+2);
#define EMIT(v) do{putword(packet+n,(v));n+=2;}while(0)
    EMIT(15);
    if((scene>>8)!=(c->scene>>8)){EMIT(17);EMIT(2);EMIT(0x8000);EMIT(scene>>8);}
    if(pcm!=65535){EMIT(13);EMIT(pcm);EMIT(10);}
    EMIT(32);EMIT(scene&255);
    if(s->bytes[4]){EMIT(7);EMIT(s->bytes[4]);}
    putword(packet+n,0);
#undef EMIT
    if(capacity<n+2)return 0;
    memcpy(queue,packet,n+2);
    c->scene=scene;
    c->display_mode=s->bytes[5]&15;c->menu_mode=(s->bytes[5]>>4)&3;
    c->pcm_suppression=(s->bytes[5]>>6)&1;
    if(s->bytes[5]&128)c->actor_pcm_suppression=1;
    return n;
}
bool resource_contact_restore_actors(const resource_contact_state *s,uint8_t actors[64*80]){
    if(!s || !actors)return false;
    for(unsigned i=0;i<64;i++){
        uint8_t *a=actors+i*80;unsigned flags=s->bytes[8+i];
        if(!word(a))continue;
        if(!(flags&128)){memset(a,0,80);continue;}
        if(flags&64)continue;
        a[0x25]=(a[0x25]&252)|(flags&3);
        a[0x24]=(a[0x24]&243)|(flags&12);
        a[0x4e]=(a[0x4e]&254)|((flags>>4)&1);
        a[0x26]=(flags>>5)&1;
    }
    return true;
}
