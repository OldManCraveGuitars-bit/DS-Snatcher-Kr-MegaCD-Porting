#include "video_filters.h"
#include "practice_expand.h"
#include "reception_background.h"
#include "resource_room.h"
#include "resource_practice_room.h"
#include "resource_archive.h"
#include "native_entry.h"
#include "music_player.h"
#include "story_audio.h"
#include <filesystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SNATCHER_ENTRY_PROFILE
#include <nds.h>
volatile unsigned factory_room_tick_cycles,factory_room_draw_cycles;
#endif
static resource_room *room;
static resource_practice practice;
static bool practice_prepared,practice_running;
static bool practice_reset,practice_reset_bound;
static unsigned practice_held,practice_pressed;
volatile unsigned practice_diagnostic[8];
unsigned reception_source_epoch;
const resource_opening_state *reception_background_video(void){return room?&room->video:NULL;}
static uint8_t *bank,*sub;
static volatile unsigned vblanks;
static unsigned origin,ticked;
static unsigned source_file;
static bool popup_active;
static bool contact_returning;
static resource_background_picture retained_picture;
void reception_background_invalidate_picture(void){
    resource_background_picture_invalidate(&retained_picture);
}
unsigned reception_generation;
unsigned reception_source_scene;
unsigned reception_draw_frame;
/* Read-only development ABI for independent VRAM/SAT pixel verification.
 * No renderer state or recorded input is embedded in this offset table. */
const unsigned reception_observe_layout[] __attribute__((used))={
    sizeof(resource_room),offsetof(resource_room,video.frames),
    offsetof(resource_room,video.front_vram),offsetof(resource_room,video.front_sat),
    offsetof(resource_room,video.front_sprites),offsetof(resource_room,video.plane_b),
    offsetof(resource_room,video.cards.hscroll),offsetof(resource_room,video.cards.vscroll),
    offsetof(resource_room,video.cards.cached_registers),offsetof(resource_room,video.raster_vertical),
    offsetof(resource_room,video.raster_visible),offsetof(resource_room,video.raster_horizontal),
    offsetof(resource_room,presentation.colors),
    offsetof(resource_room,video.scene.actors),offsetof(resource_room,video.scene.globals)};
volatile unsigned reception_error_state[9];
static bool room_sound(void *context,uint32_t entry,unsigned argument,unsigned channel,resource_scene_devices *devices){
    (void)context;
    reception_error_state[6]=entry;reception_error_state[7]=argument;reception_error_state[8]=channel;
    if(entry==0x150ee){
        if(!music_player_room_pcm_effect(argument,channel))return false;
        devices->pcm_flags|=music_player_room_pcm_flags();return true;
    }
    return entry==0x150fa && (!argument || (channel==0?music_player_effect(argument):
        channel==1 && music_player_command(argument)));
}
void reception_background_vblank(void){vblanks++;}
bool reception_background_active(void){return room!=NULL;}
bool reception_background_terminal(void){return room && source_file==12 && (room->scene==1 || room->scene==2 || room->scene==5);}
bool reception_background_supports(unsigned file){return (file>=8 && file<=16) || file==0x19 || file==0x1a || file==0x20 || file==0x29;}
bool reception_background_supports_scene(unsigned file,unsigned scene){
    /* Original SP07 K1 scenes currently used by the exploration graph.
     * K1/5 remains owned by the continuous native forecourt entry player. */
    const unsigned factory=(1u<<0)|(1u<<1)|(1u<<2)|(1u<<3)|(1u<<7)|(1u<<8)|(1u<<9)|
                           (1u<<10)|(1u<<11)|(1u<<12)|(1u<<13)|(1u<<14);
    return scene<=255 && reception_background_supports(file) && (file!=0x29 || scene==5) &&
           (file!=0x1a || scene<10) &&
           (file!=0x20 || scene==0 || scene==3 || scene==4) &&
           (file!=0x19 || (scene<32 && (factory&(1u<<scene))));
}
bool reception_background_matches(unsigned code){return room && source_file==(code>>8) && room->scene==(code&255);}
bool reception_background_presenting(unsigned code){
    return reception_background_matches(code) ||
        (room && room->contact_valid && reception_source_scene==code);
}
unsigned reception_background_contact_music(void){return room && room->contact_valid?room->contact.bytes[4]:0xffff;}
static void release_room(void){
    practice_reset=practice_reset_bound=false;
    practice_prepared=practice_running=false;practice_held=practice_pressed=0;memset(&practice,0,sizeof(practice));
    reception_source_epoch++;
    reception_background_invalidate_picture();
    if(room)resource_background_bind(NULL,0);
    free(room);free(bank);free(sub);room=NULL;bank=sub=NULL;ticked=0;source_file=reception_source_scene=reception_draw_frame=0;
    popup_active=contact_returning=false;
}
static bool bank_name(unsigned file,char name[16]){
    if(!reception_background_supports(file))return false;
    if(file==0x19)snprintf(name,16,"DATA_K1.BIN");
    else if(file==0x1a)snprintf(name,16,"DATA_M0.BIN");
    else if(file==0x20)snprintf(name,16,"DATA_S0.BIN");
    else if(file==0x29)snprintf(name,16,"DATA_Y05.BIN");
    else snprintf(name,16,"DATA_H%02u.BIN",file-8);
    return true;
}
static bool room_request(void *context,unsigned kind,uint32_t at,unsigned arg){
    (void)context;
    if(!room)return false;
    if(kind==RESOURCE_TASK_CD_PAUSE){
        /* The phone owns a room only after entry/CD playback has ended. */
        room->io.cd_status=0;return true;
    }
    if(kind==RESOURCE_TASK_PCM_START){
        room->current_pcm=arg;
        if(!contact_returning)return true; /* Main already prepared this voice/bank. */
        int type=music_player_room_pcm_load(arg);
        return type>0 || (type==0 && story_audio_begin(arg));
    }
    if(kind!=RESOURCE_TASK_FILE_START || at!=0x28000)return false;
    char name[16];if(!bank_name(arg,name))return false;
    FILE *f=fopen("nitro:/native-resource-banks-r22.nrs","rb");if(!f)return false;
    size_t bytes=0;uint8_t *next=resource_archive_load(f,name,&bytes);fclose(f);
    if(!next)return false;
    /* Task15 has completed. Retain the original VDP, actors and caller
     * context while task17 replaces only the disc bank at28000. */
    uint8_t *old=bank;bank=next;
    room->video.scene.resource=bank;room->video.scene.resource_bytes=bytes;
    source_file=arg;reception_source_epoch++;free(old);
    reception_background_invalidate_picture();return true;
}
static void room_bind_devices(void){
    room->devices.sound=room_sound;room->devices.context=NULL;
    room->io.context=NULL;room->io.request=room_request;
    room->io.cd_status=0;room->io.file_busy=false;
}
void reception_background_end(void){music_player_room_pcm_end();release_room();}
void reception_background_handoff_combat(void){release_room();}
bool reception_background_combat_image(uint16_t output[256*192]){
    return room && resource_room_draw(room,output);
}
bool reception_background_scene(unsigned scene){
    return reception_background_source(8,scene);
}
bool reception_background_source(unsigned file,unsigned scene){
    reception_background_invalidate_picture();
    if(!reception_background_supports_scene(file,scene))return false;
    if(room && room->contact_valid){
        if(!resource_room_file_scene(room,source_file,file,scene))return false;
        reception_source_scene=(file<<8)|scene;popup_active=false;return true;
    }
    if(room && source_file!=file)release_room();
    if(!room){
        if(!nitroFSInit(NULL))return false;
        FILE *f=fopen("nitro:/native-resource-banks-r22.nrs","rb");if(!f)return false;
        size_t bytes=0,subbytes=0;
        char name[16];
        if(!bank_name(file,name)){fclose(f);return false;}
        bank=resource_archive_load(f,name,&bytes);
        sub=resource_archive_load(f,"SUBCODE.BIN",&subbytes);fclose(f);
        room=malloc(sizeof(resource_entry_room_storage));
        if(!bank || !sub || !room || !resource_room_begin(room,bank,bytes,sub,subbytes,scene,0)){
            reception_background_end();return false;
        }
        origin=vblanks;ticked=0;source_file=file;reception_source_epoch++;
        room_bind_devices();
    }else if(!resource_room_scene(room,scene))return false;
    reception_source_scene=(file<<8)|scene;
    popup_active=false;
    return true;
}
bool reception_background_control(unsigned actor,unsigned command){
    return room && resource_room_control(room,actor,command);
}
bool reception_background_voice(unsigned voice){return room && resource_room_voice(room,voice);}
bool reception_background_pcm_command(unsigned command){return room && resource_room_pcm_command(room,command);}
bool reception_background_actor_pcm_suppression(unsigned value){return room && resource_room_actor_pcm_suppression(room,value);}
bool reception_background_contact_capture(void){
    if(!room || contact_returning || room->contact_valid)return false;
    room->pcm_suppression=music_player_room_pcm_attenuation();
    return resource_room_contact_capture(room,source_file,music_command);
}
bool reception_background_contact_return(unsigned *scene_key){
    if(!room || !scene_key || contact_returning || !room->contact_valid || reception_background_busy())return false;
    unsigned key=(unsigned)room->contact.bytes[0]<<8|room->contact.bytes[1];
    if(!reception_background_supports_scene(key>>8,key&255) ||
       !resource_room_contact_return(room,source_file) ||
       !music_player_room_pcm_set_attenuation(room->pcm_suppression))return false;
    *scene_key=key;reception_source_scene=key;contact_returning=true;popup_active=false;return true;
}
bool reception_background_popup(unsigned picture,unsigned position){
    if(!room || !resource_room_popup(room,picture,position))return false;
    popup_active=true;return true;
}
bool reception_background_popup_end(void){
    if(!room || !resource_room_control(room,1,2))return false;
    popup_active=false;return true;
}
bool reception_background_popup_active(void){return room && popup_active;}
bool reception_background_restore_popup(unsigned picture,unsigned position){
    if(!reception_background_popup(picture,position))return false;
    /* Reconstruct the picture without replaying its appearance sound on
     * load. Original actor9D tests this source flag before a sound request. */
    uint8_t prior=room->video.scene.globals[0x78c3-0x7860];
    room->video.scene.globals[0x78c3-0x7860]=1;
    for(unsigned i=0;i<120;i++)if(!resource_room_tick(room)){room->video.scene.globals[0x78c3-0x7860]=prior;return false;}
    room->video.scene.globals[0x78c3-0x7860]=prior;
    origin=vblanks;ticked=0;return true;
}
int reception_background_checkpoint_capture(uint8_t **output,size_t *bytes){
    *output=NULL;*bytes=0;if(!room)return 0;
    size_t n=resource_room_checkpoint_bytes(room);if(!n || n>256*1024)return -1;
    uint8_t *p=malloc(n);if(!p)return -1;
    if(!resource_room_checkpoint_export(room,source_file,popup_active,p,n) ||
       !resource_room_checkpoint_with_pcm(p,n,music_player_room_pcm_bank())){free(p);return -1;}
    *output=p;*bytes=n;return 1;
}
bool reception_background_checkpoint_restore(const uint8_t *input,size_t bytes){
    unsigned file,scene;bool popup;
    if(!resource_room_checkpoint_header(input,bytes,&file,&scene,&popup) ||
       !reception_background_supports_scene(file,scene))return false;
    reception_background_end();
    unsigned pcm=resource_room_checkpoint_pcm(input,bytes);
    if(!reception_background_source(file,scene) || !resource_room_checkpoint_import(room,input,bytes) ||
       (pcm && music_player_room_pcm_load(pcm)!=1) ||
       !reception_background_rebind()){
        reception_background_end();return false;
    }
    room_bind_devices();
    if(!music_player_room_pcm_set_attenuation(room->pcm_suppression)){reception_background_end();return false;}
    popup_active=popup;origin=vblanks;ticked=0;reception_generation++;return true;
}
bool reception_background_busy(void){return room && (room->tasks.active || room->queued || practice_reset);}
bool reception_background_rebind(void){
    reception_background_invalidate_picture();
    return room && resource_background_bind_vram(room->video.front_vram,room->video.front_palette,32,64,0xe000,room->video.plane_b,0,0,256,224);
}
bool reception_background_adopt_entry(void){
    if(!reception_background_scene(2))return false;
    size_t bytes=room->video.scene.resource_bytes,subbytes=room->video.scene.subcode_bytes;
    if(!native_entry_copy_room(room))return false;
    room->current_pcm=music_player_room_pcm_id();
    room->pcm_suppression=music_player_room_pcm_attenuation();
    room->video.scene.resource=bank;room->video.scene.resource_bytes=bytes;
    room->video.scene.subcode=sub;room->video.scene.subcode_bytes=subbytes;
    room_bind_devices();
    origin=vblanks;ticked=0;return true;
}
bool reception_background_restore(unsigned scene){
    return reception_background_restore_source(8,scene);
}
bool reception_background_restore_source(unsigned file,unsigned scene){
    reception_background_end();
    if(!reception_background_source(file,scene))return false;
    /* Source0334 resumes the large Mika group. Saves contain story state,
     * not transient GPU state: reconstruct original commands, not a bitmap. */
    if(file==8 && scene==1 && !reception_background_control(16,0))return false;
    for(unsigned i=0;i<120;i++)if(!resource_room_tick(room))return false;
    origin=vblanks;ticked=0;return true;
}
volatile unsigned reception_work_phase;
static bool reception_background_update_inner(uint8_t pcm_flags){
    if(!room)return true;
    if(practice_prepared && !practice_running)return true;
#ifdef SNATCHER_ENTRY_PROFILE
    unsigned profile_begin=cpuGetTiming();
#endif
    /* NDS VBlank is BUS_CLOCK/(355*263*6), source VBlank53693175/896040.
     * IRQ counting keeps native ticks independent of main-loop redraws. */
    unsigned target=(uint64_t)(vblanks-origin)*560190*53693175/(UINT64_C(33513982)*896040)+1;
    room->devices.pcm_flags=pcm_flags|music_player_room_pcm_flags();
    /* Never repay an unbounded wall-clock debt inside one foreground
     * update. In particular, bulk SD audio servicing per overdue practice
     * tick could add more debt than it repaid, starving touch and LCD work.
     * Keep every original tick queued, but publish and poll input between
     * small batches. The main loop already services the shared PCM stream. */
    unsigned budget=(practice_running || practice_reset)?2u:~0u;
    while(ticked<target && budget--){
        bool good;
        if(practice_reset && !room->tasks.active && !room->queued){
            if(!practice_reset_bound){
                if(!resource_practice_room_reset_begin(&practice,room))return false;
                practice_reset_bound=true;
            }
            bool complete=false;
            good=resource_practice_room_reset_tick(&practice,room,&complete);
            if(complete)practice_reset=practice_reset_bound=false;
        }else if(practice_running){
            good=resource_practice_room_tick(&practice,room,practice_held,practice_pressed);
            practice_pressed=0;
            practice_diagnostic[0]=practice.error;
            practice_diagnostic[1]=reception_background_practice_value(0x78e8);
            practice_diagnostic[2]=reception_background_practice_value(0x7908);
            practice_diagnostic[3]=reception_background_practice_value(0x790a);
            practice_diagnostic[4]=reception_background_practice_value(0x790c);
            practice_diagnostic[5]=reception_background_practice_value(0x78f8);
            practice_diagnostic[6]=reception_background_practice_value(0x78fa);
            practice_diagnostic[7]=practice.ram[0xaf];
        }else good=resource_room_tick(room);
        if(!good){
            reception_error_state[0]=room->error;reception_error_state[1]=room->video.error;
            reception_error_state[2]=room->video.scene.error;reception_error_state[3]=room->tasks.error;
            reception_error_state[4]=room->video.frames;reception_error_state[5]=room->scene;
            return false;
        }
        ticked++;reception_generation++;
        if(practice_running && resource_practice_room_done(&practice)){practice_running=false;break;}
    }
#ifdef SNATCHER_ENTRY_PROFILE
    factory_room_tick_cycles+=cpuGetTiming()-profile_begin;
#endif
    if(contact_returning && !reception_background_busy()){
        room->contact_valid=false;contact_returning=false;
    }
    return true;
}
bool reception_background_draw(uint16_t output[256*192]){
    reception_background_invalidate_picture();
    return reception_background_draw_cached(output,NULL);
}
bool reception_background_draw_cached(uint16_t output[256*192],bool *copied){
#ifdef SNATCHER_ENTRY_PROFILE
    unsigned profile_begin=cpuGetTiming();
#endif
    if(copied)*copied=false;
    if(!room || !output)return false;
    bool good=reception_background_terminal() || (source_file==14 && room->scene==0)?
        resource_opening_draw_view_indexed_rgb555_cached(&room->video,output,room->presentation.colors,8,192,&retained_picture,copied):
        resource_room_draw_native_cached(room,output,&retained_picture,copied);
    if(!good)return false;
#ifdef SNATCHER_ENTRY_PROFILE
    factory_room_draw_cycles+=cpuGetTiming()-profile_begin;
#endif
    reception_draw_frame=room->video.frames;return true;
}


bool reception_background_draw_indices(uint8_t output[256*192],uint16_t palette[64],bool *copied){
    if(copied)*copied=false;
    if(!room)return false;
    memcpy(palette,room->presentation.colors,128);
    bool good=reception_background_terminal()?
        resource_opening_draw_terminal_indices(&room->video,output,palette,&retained_picture,copied):
        source_file==14 && room->scene==0?
        resource_opening_draw_practice_indices(&room->video,output,palette,&retained_picture,copied):
        resource_room_draw_indices_cached(room,output,palette,&retained_picture,copied);
    if(!good)return false;
    reception_draw_frame=room->video.frames;return true;
}

bool reception_background_gameover(uint8_t out[256*192],uint16_t palette[64]){
 if(!room || source_file!=0x29 || room->scene!=5)return false;
 memcpy(palette,room->presentation.colors,128);
 return resource_opening_draw_full_indices_cached(&room->video,out,palette,&retained_picture,NULL);
}

bool reception_background_begin_return(uint8_t fields[128]){
    if(!room || source_file!=25 || room->scene!=0 || reception_background_busy() ||
       !music_player_return_begin(room,fields))return false;
    /* The native player now owns all three allocations. Reset only this
     * adapter's metadata; freeing them here would destroy the live return. */
    room=NULL;bank=sub=NULL;
    /* Discard only rebuildable compositor caches, as release_room did before
     * the transfer. The native VDP/banks above stay owned by the entry. */
    resource_background_bind(NULL,0);
    release_room();return native_entry_return_rebind() && native_entry_return_ready() && music_player_return_reserve();
}
bool reception_background_finish_return(void){
    if(room || bank || sub)return false;
    /* Reuse the allocation that was given to the native return. A second
     * full VDP allocation is unnecessary and may fail on a fragmented heap. */
    if(!native_entry_take_return_room(&room,&bank,&sub))return false;
    /* The native player already owns these clean original resources.
     * No I/O or Maxmod callback may run with its history released. */
    room->video.scene.resource=bank;room->video.scene.subcode=sub;
    room->current_pcm=music_player_room_pcm_id();
    room->pcm_suppression=music_player_room_pcm_attenuation();
    room_bind_devices();
    source_file=16;popup_active=false;reception_source_epoch++;reception_generation++;
    origin=vblanks;ticked=0;reception_source_scene=0x1003;return true;
}

bool reception_background_practice_begin(void){
    if(practice_reset)return false;
    if(!room || source_file!=14 || !resource_practice_room_begin(&practice,room))return false;
    practice_prepared=true;practice_running=false;practice_held=practice_pressed=0;
    memset((void*)practice_diagnostic,0,sizeof(practice_diagnostic));return true;
}
bool reception_background_practice_reset_begin(void){
    if(!room || source_file!=14 || room->scene!=0 || practice_prepared || practice_running || practice_reset)return false;
    practice_reset=true;practice_reset_bound=false;return true;
}
void reception_background_practice_run(void){
    if(practice_prepared && !practice_running && !resource_practice_room_done(&practice)){practice_running=true;origin=vblanks;ticked=0;}
}
void reception_background_practice_input(unsigned held,unsigned pressed){
    if(practice_running){practice_held=held&15;practice_pressed|=pressed&96;}
}
unsigned reception_background_practice_value(unsigned address){
    if(!practice_prepared || address<0x7800 || address>0x79fe)return 0;
    const uint8_t *p=practice.ram+address-0x7800;return (unsigned)p[0]<<8|p[1];
}
bool reception_background_practice_done(void){return practice_prepared && resource_practice_room_done(&practice);}
bool reception_background_practice_finish(uint16_t result[7]){
    if(!result || !reception_background_practice_done())return false;
    static const unsigned addresses[7]={0x7908,0x790a,0x790c,0x790e,0x7914,0x7916,0x7918};
    for(unsigned i=0;i<7;i++)result[i]=reception_background_practice_value(addresses[i]);
    practice_prepared=practice_running=false;origin=vblanks;ticked=0;return true;
}
/* One continuous full-screen projection, including both side panels.
 * No detached/repeated wall strips: original actors, doors and their
 * supporting fixtures remain connected throughout every native frame. */
static uint16_t *practice_output;
static bool practice_output_valid,practice_output_warm;
static bool reception_background_practice_draw_inner(uint16_t output[256*192],uint8_t scratch[256*192],
    bool warm,bool force,bool *changed){
    bool copied=false;
    if(!room || source_file!=14 || room->scene!=0 ||
       !resource_opening_draw_practice_indices(&room->video,scratch,room->presentation.colors,&retained_picture,&copied))return false;
    /* The source lease includes both pixel and palette changes. Only the
     * combat caller retains its completed output; all other callers force
     * a rewrite because transition/UI buffers have independent owners. */
    bool update=force || copied || !practice_output_valid || practice_output!=output || practice_output_warm!=warm;
    if(update){
        uint16_t palette[64];
        for(unsigned i=0;i<64;i++)palette[i]=warm?video_filters_warm(room->presentation.colors[i]):room->presentation.colors[i];
        practice_expand_rgb555(scratch,palette,output);
        practice_output=output;practice_output_warm=warm;practice_output_valid=true;
    }
    if(changed)*changed=update;
    reception_draw_frame=room->video.frames;return true;
}
int reception_background_practice_touch(int x,int y){
    if(x<0 || x>=256 || y<0 || y>=192)return -1;
    unsigned sx=16+(unsigned)x*224/256,sy=8+(unsigned)y*128/192;
    return (sx<96?0:sx<160?1:2)+(sy<48?0:sy<88?3:6);
}


bool reception_background_update(uint8_t pcm_flags){
    reception_work_phase=1;
    bool good=reception_background_update_inner(pcm_flags);
    reception_work_phase=0;return good;
}
bool reception_background_practice_draw(uint16_t output[256*192],uint8_t scratch[256*192]){
    reception_work_phase=2;
    bool good=reception_background_practice_draw_inner(output,scratch,false,true,NULL);
    reception_work_phase=0;return good;
}

bool reception_background_practice_draw_filtered(uint16_t output[256*192],uint8_t scratch[256*192],bool warm,bool force,bool *changed){
    reception_work_phase=2;
    bool good=reception_background_practice_draw_inner(output,scratch,warm,force,changed);
    reception_work_phase=0;return good;
}
