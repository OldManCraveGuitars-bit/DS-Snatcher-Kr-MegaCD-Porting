#include "resource_room.h"
#include <string.h>
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static void putword(uint8_t *p,unsigned n){p[0]=n>>8;p[1]=n;}
static bool enqueue(resource_room *r,const uint8_t *p,unsigned bytes){
    if(bytes>510-r->queued)return false;
    memcpy(r->queue+r->queued,p,bytes);r->queued+=bytes;
    memset(r->queue+r->queued,0,2);return true;
}
bool resource_room_scene(resource_room *r,unsigned scene){
    if(!r || scene>255 || r->error)return false;
    /* Original SUB17314 compares the cached file/scene key and suppresses
     * repeated requests, independently of voice-task subscene changes. */
    if(scene==r->scene)return true;
    const uint8_t commands[]={0,15,0,18,0,(uint8_t)scene};
    if(!enqueue(r,commands,sizeof(commands)))return false;
    r->scene=scene;return true;
}
bool resource_room_control(resource_room *r,unsigned actor,unsigned command){
    if(!r || r->error || actor>=64 || command>5)return false;
    const uint8_t commands[]={0,9,0,(uint8_t)command,0,(uint8_t)actor};
    return enqueue(r,commands,sizeof(commands));
}
bool resource_room_menu_return(resource_room *r){
    if(!r || r->error)return false;
    /* SUB16850/1398E clears voice actors before and after the remaining
     * work queue. Keep the callback in queue order, including old saves. */
    const uint8_t command[]={0,34,0,1,0x39,0xd0};
    if(!enqueue(r,command,sizeof(command)))return false;
    resource_scene_clear_voice_actors(&r->video.scene);return true;
}
bool resource_room_file_scene(resource_room *r,unsigned current_file,unsigned file,unsigned scene){
    if(!r || r->error || current_file>255 || file>255 || scene>255)return false;
    if(current_file==file)return resource_room_scene(r,scene);
    const uint8_t packet[]={0,15,0,17,0,2,0x80,0,0,(uint8_t)file,0,18,0,(uint8_t)scene};
    if(!enqueue(r,packet,sizeof(packet)))return false;
    r->scene=scene;return true;
}
bool resource_room_voice(resource_room *r,unsigned voice){
    if(!r || r->error || voice>65535)return false;
    /* SUB173AE/101E2 pass the complete word to PCM and the voice scene table. */
    const uint8_t commands[]={0,13,(uint8_t)(voice>>8),(uint8_t)voice};
    if(!enqueue(r,commands,sizeof(commands)))return false;
    r->current_pcm=voice;return true;
}
bool resource_room_suppress_actor_effects(resource_room *r){
    if(!r || r->error)return false;
    const uint8_t packet[]={0,4};return enqueue(r,packet,sizeof(packet));
}
bool resource_room_pcm_wait(resource_room *r){
    if(!r || r->error)return false;
    const uint8_t packet[]={0,10};return enqueue(r,packet,sizeof(packet));
}
bool resource_room_pcm_command(resource_room *r,unsigned command){
    if(!r || r->error || command>65535)return false;
    const uint8_t packet[]={0,12,(uint8_t)(command>>8),(uint8_t)command};
    return enqueue(r,packet,sizeof(packet));
}
bool resource_room_actor_pcm_suppression(resource_room *r,unsigned value){
    if(!r || r->error || value>65535)return false;
    const uint8_t packet[]={0,29,(uint8_t)(value>>8),(uint8_t)value};
    return enqueue(r,packet,sizeof(packet));
}
bool resource_room_popup(resource_room *r,unsigned picture,unsigned position){
    if(!r || r->error || picture>255 || position>=18)return false;
    const uint8_t commands[]={0,21,0,(uint8_t)picture,0,(uint8_t)position};
    return enqueue(r,commands,sizeof(commands));
}
static bool request(void *ctx,unsigned kind,uint32_t at,unsigned value){
    resource_room *r=ctx;
    if(kind==RESOURCE_TASK_ACTOR_RESTORE)
        return r->contact_valid && resource_contact_restore_actors(&r->contact,r->video.scene.actors);
    if(r->io.request)return r->io.request(r->io.context,kind,at,value);
    /* Story audio owns the real PCM playback. Task13 still performs the
     * original voice scene selection and actor handshake. */
    return kind==RESOURCE_TASK_PCM_START;
}
bool resource_room_contact_capture(resource_room *r,unsigned file,unsigned music){
    if(!r || r->error || file>255 || r->scene>255 || music>255)return false;
    resource_contact_context context={
        .scene=(file<<8)|r->scene,.pcm=r->current_pcm,.music=music,
        .display_mode=r->video.scene.picture_mode,.menu_mode=r->menu_mode,
        .pcm_suppression=r->pcm_suppression,.actor_pcm_suppression=r->video.scene.globals[0x30]};
    if(!resource_contact_capture(&r->contact,&context,r->video.scene.actors))return false;
    r->contact_valid=true;return true;
}
bool resource_room_contact_return(resource_room *r,unsigned file){
    if(!r || r->error || !r->contact_valid || file>255 || r->scene>255)return false;
    resource_contact_context context={.scene=(file<<8)|r->scene,
        .actor_pcm_suppression=r->video.scene.globals[0x30]};
    uint8_t packet[26];size_t bytes=resource_contact_return(&r->contact,&context,packet,sizeof(packet));
    if(!bytes || !enqueue(r,packet,bytes))return false;
    r->scene=context.scene&255;r->video.scene.picture_mode=context.display_mode;
    r->menu_mode=context.menu_mode;r->pcm_suppression=context.pcm_suppression;
    r->video.scene.globals[0x30]=context.actor_pcm_suppression;return true;
}
bool resource_room_begin(resource_room *r,const void *bank,size_t bytes,
    const void *sub,size_t subbytes,unsigned scene,uint32_t rng){
    if(!r || scene>255)return false;
    memset(r,0,sizeof(*r));r->scene=256;r->current_pcm=65535;
    if(!resource_opening_bind_scene(&r->video,bank,bytes,sub,subbytes,rng))return false;
    /* The real game keeps F600's boot/font colours across DATA_H/K loads.
     * This standalone room context cannot inherit the preceding player's
     * CRAM. Read the original setup instructions instead of leaving the
     * shared colours black; built-in item pictures use palette0 as well. */
    if(!resource_scene_shared_palette(&r->video.scene))return false;
    /* The same original SUBCODE initialization used by resource_entry.
     * Font actor0 initializes native palette/flags but is excluded from SAT. */
    const uint8_t initial[]={0,1,0,0,0xed,0x0e,0,3,0,0,0xec,0xb6,0,3,0,0,0xec,0xc4,0,0};
    return resource_tasks_begin(&r->tasks,initial,sizeof(initial)) && resource_room_scene(r,scene);
}
bool resource_room_tick(resource_room *r){
    if(!r || r->error)return false;
    resource_opening_state *v=&r->video;
    if(!resource_opening_publish(v)){r->error=1;return false;}
    uint8_t *g=v->scene.globals;
    uint32_t rng=(uint32_t)word(g+10)<<16|word(g+12);
    resource_actor_random(&rng);putword(g+10,rng>>16);putword(g+12,rng);
    putword(g+0x52,1);r->devices.frame_clock=v->frames;
    if(!r->tasks.active && r->queued){
        /* Original opcode3D in the same file: task15 clear, task18 load.
         * Clear/load timing remains in the source queue implementation. */
        if(!resource_tasks_begin(&r->tasks,r->queue,r->queued+2)){r->error=2;return false;}
        r->queued=0;v->scene_id=r->scene;
    }
    resource_tasks_io io=r->io;io.context=r;io.request=request;
    /* Original SUB D6EE/D6F6 skips FFC4 while plane clearing or tile DMA
     * is active. Otherwise a queued voice scene replaces the unfinished
     * large-picture load, leaving only its eye/mouth sprite tiles. */
    if(!g[0x27] && !g[0x26] && !resource_tasks_tick(&r->tasks,&v->scene,&r->devices,&io)){r->error=3;return false;}
    if(!g[0x27] && !g[0x26] && !resource_scene_actors_tick_devices(&v->scene,&r->devices)){
        r->error=4;return false;
    }
    resource_flashlight_present(r->flashlight,r->menu_mode,g,&v->scene.scroll);
    if(!resource_opening_finish_tick(v)){r->error=5;return false;}
    if(!resource_opening_presentation_tick(&r->presentation,v)){r->error=6;return false;}
    return true;
}
bool resource_room_draw(resource_room *r,uint16_t out[256*192]){
    return r && !r->error && resource_opening_draw_area_compact(&r->video,out,r->presentation.colors,32,8,192,128);
}
bool resource_room_draw_native(resource_room *r,uint16_t out[256*192]){
    return resource_room_draw_native_cached(r,out,NULL,NULL);
}
bool resource_room_draw_native_cached(resource_room *r,uint16_t out[256*192],
    resource_background_picture *picture,bool *copied){
    /* Native rows AND columns: avoid two nearest-neighbour passes losing
     * face detail. Keep the192x128 picture at x32; transparent margins
     * discard the original border/text/HUD before the single affine fit. */
    /* Use the same original plane/SAT composition, cached as native colour
     * numbers. Palette-only frames no longer recolour every plane/sprite;
     * final RGB expansion happens once after priority/ownership resolution. */
    bool changed=false;if(copied)*copied=false;
    if(!r || r->error || !out || !resource_opening_draw_view_indexed_rgb555_cached(&r->video,out,r->presentation.colors,8,192,picture,&changed))return false;
    if(!changed)return true;
    for(unsigned y=0;y<128;y++){
        memset(out+y*256,0,32*sizeof(*out));
        memset(out+y*256+224,0,32*sizeof(*out));
    }
    memset(out+256*128,0,256*64*sizeof(*out));
    if(copied)*copied=true;
    return true;
}


bool resource_room_draw_indices_cached(resource_room *r,uint8_t out[256*192],uint16_t palette[64],
    resource_background_picture *picture,bool *copied){
    if(copied)*copied=false;
    if(!r || r->error || !palette || !resource_opening_draw_room_indices_cached(&r->video,out,r->presentation.colors,picture,copied))return false;
    memcpy(palette,r->presentation.colors,64*sizeof(*palette));return true;
}

bool resource_room_cd_begin(resource_room *r,unsigned track){
    if(!r || r->error || !track || track>99)return false;
    const uint8_t commands[]={0,14,0,(uint8_t)track};
    return enqueue(r,commands,sizeof(commands));
}
bool resource_room_cd_wait(resource_room *r){
    if(!r || r->error)return false;
    const uint8_t commands[]={0,11};return enqueue(r,commands,sizeof(commands));
}
void resource_room_cd_clock(resource_room *r,uint32_t clock){
    if(!r)return;
    for(unsigned i=0;i<4;i++)r->video.scene.globals[0x787a-0x7860+i]=clock>>(24-i*8);
}

bool resource_room_cd_time(resource_room *r,unsigned minute,unsigned second,unsigned frame){
    if(!r || r->error || minute>65535 || second>65535 || frame>65535)return false;
    const uint8_t commands[]={0,27,minute>>8,minute,second>>8,second,frame>>8,frame};
    return enqueue(r,commands,sizeof(commands));
}

bool resource_room_flashlight_mode(resource_room *r,unsigned mode){
    if(!r || r->error || mode>1)return false;
    /* Original custom10 at175A0 clears positions only when entering mode1. */
    r->menu_mode=mode;if(mode==1)memset(r->flashlight,0,8);return true;
}
bool resource_room_flashlight_input(resource_room *r,unsigned held,unsigned history){
    if(!r || r->error || r->menu_mode!=1)return false;
    uint8_t lock=0;
    return resource_flashlight_input(r->flashlight,r->video.scene.subcode,
        r->video.scene.subcode_bytes,held,history,&lock,r->video.scene.actors+0x25);
}
bool resource_room_flashlight_touch(resource_room *r,int x,int y){
    return r && !r->error && r->menu_mode==1 && resource_flashlight_touch(r->flashlight,x,y);
}
