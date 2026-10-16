#include "resource_tasks.h"
#include <string.h>
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static void putword(uint8_t *p,unsigned v){p[0]=v>>8;p[1]=v;}
static void putlong(uint8_t *p,uint32_t v){putword(p,v>>16);putword(p+2,v);}
static uint8_t *g(resource_scene_state *s,unsigned at){return s->globals+at-0x7860;}
static const uint8_t *read_source(resource_scene_state *s,uint32_t at,size_t bytes){
    size_t available=0;const uint8_t *p=resource_scene_resolve(s,at,&available);
    return p && bytes<=available?p:NULL;
}
static bool error(resource_tasks_state *s,unsigned reason){s->error=reason;return false;}
bool resource_tasks_begin(resource_tasks_state *s,const void *data,size_t bytes){
    if(!s || !data || !bytes || (bytes&1) || bytes>sizeof(s->commands))return false;
    memset(s,0,sizeof(*s));memcpy(s->commands,data,bytes);s->active=1;return true;
}
static bool request(const resource_tasks_io *io,unsigned kind,uint32_t at,unsigned arg){
    return io && io->request && io->request(io->context,kind,at,arg);
}
static bool spawn(resource_scene_state *s,uint32_t at){
    for(unsigned budget=0;budget<64;budget++,at+=12){
        const uint8_t *p=read_source(s,at,1);if(!p)return false;
        if(*p==255)return true;
        p=read_source(s,at,12);if(!p || *p>=64)return false;
        uint8_t *a=s->actors+p[0]*80;
        memset(a,0,80);putword(a,1);a[0x25]=p[1];putword(a+6,word(p+2));putword(a+10,word(p+4));
        putlong(a+0x2e,longword(p+6));putlong(a+0x32,at+12+(int16_t)word(p+10));putword(a+0x28,65535);
        /* Unlike F708 scene packets,1004E leaves map36 unresolved until
         * the next native actor pass. This also initializes shared actor0. */
    }
    return false;
}
bool resource_tasks_tick(resource_tasks_state *q,resource_scene_state *s,resource_scene_devices *d,const resource_tasks_io *io){
    if(!q || !s || !s->resource || !d)return false;
    q->error=0;if(!q->active)return true;
    for(unsigned budget=0;budget<256;budget++){
        if((q->index&1) || q->index>510)return error(q,1);
        unsigned op=word(q->commands+q->index),length;
        switch(op){
        case 0:case 6:case 10:case 11:case 15:case 19:case 25:length=2;break;
        case 7:case 12:case 13:case 14:case 16:case 18:case 28:case 29:case 32:length=4;break;
        case 1:case 3:case 9:case 21:length=6;break;
        case 17:length=8;break;
        default:return error(q,2);
        }
        if(length>sizeof(q->commands)-q->index)return error(q,1);
        const uint8_t *p=q->commands+q->index+2;unsigned arg=length>=4?word(p):0;
        switch(op){
        case 0:
            memset(q->commands,0,sizeof(q->commands));q->index=0;q->active=0;
            putword(g(s,0x7862),0);return true;
        case 1:if(!spawn(s,longword(p)))return error(q,3);break;
        case 3:if(!resource_graphics_enqueue(&s->graphics,resource_scene_resolve,s,longword(p)))return error(q,3);break;
        case 29:*g(s,0x7890)=(uint8_t)arg;break;
        case 6:break; /* Native1011A is an immediate no-op. */
        case 7:
            if(!d->sound)return error(q,4);
            d->music=(uint8_t)arg==255?0:arg;
            if(!d->sound(d->context,0x150fa,arg,1,d))return error(q,4);
            break;
        case 28:
            if(!d->sound)return error(q,4);
            d->effect=(uint8_t)arg==0x3d?0:arg;
            if(!d->sound(d->context,0x150fa,arg,0,d))return error(q,4);
            break;
        case 12:
            /* Original101BC suppresses this request while actor PCM is
             * blocked or channels0..2 are busy, then101CE uses D1=-1. */
            if(*g(s,0x7890) || (d->pcm_flags&7))break;
            if(!d->sound || !d->sound(d->context,0x150ee,arg,255,d))return error(q,4);
            break;
        case 19:case 25:
            /* SUB1033C/103A2: poll the original completion register.
             * These are source fail-safe limits, not guessed cue lengths. */
            if(!q->phase){q->timeout=op==19?2700:600;q->phase++;}
            q->timeout--;
            if(q->timeout && (op==19?d->music:d->effect))return true;
            q->timeout=q->phase=0;break;
        case 9:{
            bool done;
            if(!resource_actor_control(s->actors,word(p+2),arg,&done))return error(q,3);
            if(!done)return true;
            break;
        }
        case 10:
            if((int16_t)(q->phase-1)>0){q->phase=0;break;}
            if(q->phase!=1){q->timeout=3600;q->phase++;}
            q->timeout--;
            if(!q->timeout || !(d->pcm_flags&15)){q->timeout=0;q->phase++;}
            return true;
        case 11:
            /* Original101A6 tests the high status byte. A ready/playing
             * disc (0100) still waits; stopped/paused (0000/0500) completes. */
            if(!io || ((io->cd_status&0xff00)!=0 && (io->cd_status&0xff00)!=0x500))return true;
            break;
        case 14:
            if(!io)return error(q,4);
            if(!q->phase){
                if(!request(io,RESOURCE_TASK_CD_START,0,arg))return error(q,4);
                q->phase++;return true;
            }
            if(q->phase==1){
                if(io->cd_status!=0x100)return true;
                putlong(g(s,0x787a),0);
            }else if(q->phase==2){q->phase++;return true;}
            q->phase=0;break;
        case 13:{
            if(!q->phase){
                if(!request(io,RESOURCE_TASK_PCM_START,0,arg))return error(q,4);
                q->caption_reset=255;q->phase++;return true;
            }
            if(q->phase==1){q->phase++;return true;}
            const uint8_t *scene=read_source(s,0x13fec+arg,1);
            if(!scene)return error(q,3);
            if(q->phase==2){
                if(*scene!=255)*g(s,0x7891)=1;
                if(*g(s,0x7891)){*g(s,0x78c7)=1;q->phase++;return true;}
            }else if(*scene!=255){
                *g(s,0x78bc)=1;bool good=resource_scene_load(s,*scene);*g(s,0x78bc)=0;
                if(!good)return error(q,3);
            }
            q->phase=0;break;
        }
        case 15:
            if(q->phase){q->phase=0;break;}
            if(!((*g(s,0x788b)&127)|(*g(s,0x788a)&1))){
                if(!resource_scene_blackout(s))return error(q,3);
            }else{
                q->phase++;
                if(!resource_scene_clear(s))return error(q,3);
            }
            return true;
        case 16:
            q->counter++;
            if(q->counter!=arg)return true;
            q->counter=0;break;
        case 17:
            if(!io)return error(q,4);
            if(!q->phase){
                q->phase++;
                if(!request(io,RESOURCE_TASK_CD_PAUSE,0,0))return error(q,4);
                return true;
            }
            if(q->phase!=2){
                unsigned status=(uint16_t)io->cd_status&0xff00;
                if(io->cd_status<0 || (status && status!=0x500))return true;
                q->phase++;
                if(!request(io,RESOURCE_TASK_FILE_START,longword(p),word(p+4)))return error(q,4);
                return true;
            }
            if(io->file_busy || io->cd_status<0 || (io->cd_status&13))return true;
            putword(g(s,0x787e),0);q->phase=0;break;
        case 18:if(!resource_scene_load(s,arg))return error(q,3);break;
        case 32:
            /* Original105E8: F68E loads the scene, F7F0 performs the first
             * actor pass, then1507A reapplies the compact caller flags.
             * Applying flags before that pass changes scripts and poses. */
            if(!resource_scene_load(s,arg) || !resource_scene_actors_tick_devices(s,d))return error(q,3);
            if(!request(io,RESOURCE_TASK_ACTOR_RESTORE,0,0))return error(q,4);
            break;
        case 21:
            if(*g(s,0x7886))return true;
            if(!resource_scene_popup(s,arg,word(p+2)))return error(q,3);
            break;
        }
        q->index+=length;
    }
    return error(q,5);
}
