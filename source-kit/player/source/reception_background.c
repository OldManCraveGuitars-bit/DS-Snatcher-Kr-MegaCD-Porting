#include "system_perf.h"
#include "video_filters.h"
#include "practice_expand.h"
#include "reception_background.h"
#include "resource_room.h"
#include "resource_practice_room.h"
#include "resource_archive.h"
#include "native_entry.h"
#include "music_player.h"
#include "story_audio.h"
#include "story_vm.h"
#include <filesystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SNATCHER_ENTRY_PROFILE
#include <nds.h>
volatile unsigned factory_room_tick_cycles,factory_room_draw_cycles;
#endif
static resource_room *room;
bool reception_background_random_state(uint32_t *state,bool store){
    if(!room || room->error || !state)return false;
    uint8_t *p=room->video.scene.globals+10;
    if(store){for(unsigned i=0;i<4;i++)p[i]=*state>>(24-i*8);}
    else{*state=0;for(unsigned i=0;i<4;i++)*state=(*state<<8)|p[i];}
    return true;
}
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
static bool checkpoint_audio_pending;
static unsigned checkpoint_pcm;
static resource_background_picture retained_picture;
/* A requested scene is queued before its pixels are published. Keep the
 * previous crop until the native plane clear reaches the front buffer;
 * otherwise an END card's taller crop exposes the preceding portrait/HUD.
 * Scenes that retain their plane commit when the native queue completes. */
static unsigned viewport_key,viewport_target,viewport_clear;
static bool viewport_pending;
static unsigned published_viewport(void){
    if(viewport_pending && room && ((source_file<<8)|room->scene)==viewport_target &&
       (room->video.published_clears!=viewport_clear || (!room->tasks.active && !room->queued))){
        viewport_key=viewport_target;viewport_pending=false;
    }
    return viewport_key;
}
static void request_viewport(unsigned key){
    if(!viewport_pending)viewport_clear=room->video.published_clears;
    viewport_target=key;viewport_pending=key!=viewport_key;
}
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
bool reception_background_display_mode(unsigned value){
    if(!room || room->error || value>65535)return false;
    room->video.scene.picture_mode=value;return true;
}
/* Preserve the approved ordinary192x128 composition. Source plane
 * overflow is offscreen data, not a reason to shrink the visible picture. */
static bool wide_picture(void){
    if(!room)return false;
    switch(published_viewport()){
        return true;
    default:return false;
    }
}
/* Y15's first four farewell shots are integral original224-line art.
 * Later Y15 conversations return to the ordinary illustration/face layout. */
bool reception_background_fullframe(void){return false;}
static unsigned cinematic_source_y(void){
    unsigned key=published_viewport(),file=key>>8,scene=key&255;
    if(wide_picture())return 8;
    if(file==0x33 && scene<=3)return 16;
    if(file==22)return 28;
    if(file==6 || file==21)return 20;
    if(file==4)return 24;
    return 0;
}
bool reception_background_terminal(void){unsigned key=published_viewport();return room && (key==0x0c01 || key==0x0c02 || key==0x0c05);}
bool reception_background_supports(unsigned file){return (file>=0x2a && file<=0x33) || file==0x27 || file==0x28 || file==0x25 || file==0x26 || file==4 || (file>=0x12 && file<=0x15) || file==0x23 || file==0x24 || file==0x1f || file==0x1e || file==0x22 || file==0x1d || file==1 || file==17 || file==22 || file==5 || file==6 || file==0x17 || file==0x1c || file==0x1b || file==0x18 || file==0 || (file>=7 && file<=16) || file==0x19 || file==0x1a || file==0x20 || file==0x21 || file==0x29;}
bool reception_background_supports_scene(unsigned file,unsigned scene){
    /* Original SP07 K1 scenes currently used by the exploration graph.
     * K1/5 remains owned by the continuous native forecourt entry player. */
    const unsigned factory=(1u<<0)|(1u<<1)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<7)|(1u<<8)|(1u<<9)|
                           (1u<<10)|(1u<<11)|(1u<<12)|(1u<<13)|(1u<<14);
    return scene<=255 && (file!=0x2a || scene<=7) && (file!=0x2b || scene<=5) && (file!=0x2c || scene<=6) && (file!=0x2d || scene<=4) && (file!=0x2e || scene<=5) && (file!=0x2f || scene<=4) && (file!=0x30 || scene<=6) && (file!=0x31 || scene<=8) && (file!=0x32 || scene<=10) && (file!=0x33 || scene<=8) && (file!=0x27 || scene<=4) && (file!=0x28 || scene<=6) && (file!=0x25 || scene<=9) && (file!=0x26 || scene<=15) && (file!=4 || scene==5 || scene==6) && (file!=0x12 || scene<=2) && (file!=0x13 || scene<=2) && (file!=0x14 || scene==0) && (file!=0x15 || scene<=6) && (file!=0x23 || scene<=1) && (file!=0x24 || scene<=5) && (file!=0x1e || scene<=7) && (file!=0x1f || scene<=16) && (file!=0x22 || scene<=9) && (file!=0x1d || (scene>=1 && scene<=8) || (scene>=10 && scene<=14)) && (file!=1 || scene==0 || scene==2) && (file!=17 || scene<=17) && (file!=22 || scene<=3) && (file!=5 || scene<=11) && (file!=6 || scene<=10) && (file!=0x17 || scene<=8) && (file!=0x1c || scene<=7) && (file!=0x1b || scene<=5) && (file!=0x18 || scene<=2) && (file!=0 || scene<=3) && (file!=7 || scene<=14) && reception_background_supports(file) && (file!=0x29 || scene<=5) &&
           (file!=0x1a || scene<10) &&
           (file!=0x20 || scene<=4) &&
           (file!=0x21 || scene<=3) &&
           (file!=0x19 || (scene<32 && (factory&(1u<<scene))));
}
bool reception_background_matches(unsigned code){return room && source_file==(code>>8) && room->scene==(code&255);}
bool reception_background_presenting(unsigned code){
    return reception_background_matches(code) ||
        (room && room->contact_valid && reception_source_scene==code);
}
unsigned reception_background_contact_music(void){return room && room->contact_valid?room->contact.bytes[4]:0xffff;}
static void release_room(void){
    uint32_t last_rng;
    if(reception_background_random_state(&last_rng,false))story_set_random_state(last_rng);
    practice_reset=practice_reset_bound=false;
    practice_prepared=practice_running=false;practice_held=practice_pressed=0;memset(&practice,0,sizeof(practice));
    reception_source_epoch++;
    reception_background_invalidate_picture();
    if(room)resource_background_bind(NULL,0);
    free(room);free(bank);free(sub);room=NULL;bank=sub=NULL;ticked=0;source_file=reception_source_scene=reception_draw_frame=0;
    popup_active=contact_returning=checkpoint_audio_pending=false;checkpoint_pcm=0;
    viewport_key=viewport_target=viewport_clear=0;viewport_pending=false;
}
static bool bank_name(unsigned file,char name[16]){
    if(!reception_background_supports(file))return false;
    if((file>=0x2a && file<=0x33) || file==0x27 || file==0x28)snprintf(name,16,"DATA_Y%02u.BIN",file-0x24);
    else if(file==0x25 || file==0x26)snprintf(name,16,"DATA_Y%02u.BIN",file-0x25);
    else if(file==4)snprintf(name,16,"DATA_D2.BIN");
    else if(file>=0x12 && file<=0x15)snprintf(name,16,"DATA_H%02u.BIN",file-7);
    else if(file==0x23)snprintf(name,16,"DATA_T0.BIN");
    else if(file==0x24)snprintf(name,16,"DATA_U0.BIN");
    else if(file==0x1f)snprintf(name,16,"DATA_Q8.BIN");
    else if(file==0x1e)snprintf(name,16,"DATA_Q5.BIN");
    else if(file==0x22)snprintf(name,16,"DATA_S2.BIN");
    else if(file==0x1d)snprintf(name,16,"DATA_Q3.BIN");
    else if(file==1)snprintf(name,16,"DATA_B0.BIN");
    else if(file==22)snprintf(name,16,"DATA_H15.BIN");
    else if(file==5)snprintf(name,16,"DATA_F0.BIN");
    else if(file==6)snprintf(name,16,"DATA_F4.BIN");
    else if(file==0x17)snprintf(name,16,"DATA_I0.BIN");
    else if(file==0x1c)snprintf(name,16,"DATA_P0.BIN");
    else if(file==0x1b)snprintf(name,16,"DATA_O0.BIN");
    else if(file==0x18)snprintf(name,16,"DATA_J0.BIN");
    else if(file==0)snprintf(name,16,"DATA_A0.BIN");
    else if(file==7)snprintf(name,16,"DATA_G0.BIN");
    else if(file==0x19)snprintf(name,16,"DATA_K1.BIN");
    else if(file==0x1a)snprintf(name,16,"DATA_M0.BIN");
    else if(file==0x20)snprintf(name,16,"DATA_S0.BIN");
    else if(file==0x21)snprintf(name,16,"DATA_S1.BIN");
    else if(file==0x29)snprintf(name,16,"DATA_Y05.BIN");
    else snprintf(name,16,"DATA_H%02u.BIN",file-8);
    return true;
}
static bool room_request(void *context,unsigned kind,uint32_t at,unsigned arg){
    (void)context;
    if(!room)return false;
    if(kind==RESOURCE_TASK_CD_START)return music_player_story_cd_begin(arg);
    if(kind==RESOURCE_TASK_CD_PAUSE){
        if(!music_player_story_cd_pause())return false;
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
    published_viewport();
    if(room && room->contact_valid){
        if(!resource_room_file_scene(room,source_file,file,scene))return false;
        request_viewport((file<<8)|scene);
        reception_source_scene=(file<<8)|scene;popup_active=false;return true;
    }
    if(room && source_file!=file)release_room();
    if(!room){
        if(!nitroFSInit(NULL))return false;
        FILE *f=fopen("nitro:/native-resource-banks-r22.nrs","rb");if(!f)return false;
        size_t bytes=0,subbytes=0;
        char name[16];
        if(!bank_name(file,name)){fclose(f);return false;}
        /* The released old room leaves an exact180KiB gap. Reserve that
         * largest block first, before a smaller disc bank splits the gap.
         * Keep the global room NULL during serviced file reads: callbacks
         * must never observe this uninitialized allocation. */
        resource_room *next_room=malloc(sizeof(resource_entry_room_storage));
        bank=next_room?resource_archive_load(f,name,&bytes):NULL;
        sub=bank?resource_archive_load(f,"SUBCODE.BIN",&subbytes):NULL;fclose(f);
        uint32_t next_rng=story_random_state();
        if(!bank || !sub || !next_room || !resource_room_begin(next_room,bank,bytes,sub,subbytes,scene,next_rng)){
            free(next_room);reception_background_end();return false;
        }
        room=next_room;
        origin=vblanks;ticked=0;source_file=file;reception_source_epoch++;
        viewport_key=(file<<8)|scene;viewport_pending=false;
        room_bind_devices();
    }else{
        if(!resource_room_scene(room,scene))return false;
        request_viewport((file<<8)|scene);
    }
    reception_source_scene=(file<<8)|scene;
    popup_active=false;
    return true;
}
bool reception_background_control(unsigned actor,unsigned command){
    return room && resource_room_control(room,actor,command);
}
bool reception_background_menu_return(void){return !room || resource_room_menu_return(room);}
bool reception_background_voice(unsigned voice){return room && resource_room_voice(room,voice);}
bool reception_background_pcm_wait(void){return room && resource_room_pcm_wait(room);}
bool reception_background_suppress_actor_effects(void){return room && resource_room_suppress_actor_effects(room);}
bool reception_background_pcm_command(unsigned command){return room && resource_room_pcm_command(room,command);}
bool reception_background_actor_pcm_suppression(unsigned value){return room && resource_room_actor_pcm_suppression(room,value);}
bool reception_background_contact_capture(void){
    if(!room || contact_returning || room->contact_valid)return false;
    room->pcm_suppression=music_player_room_pcm_attenuation();
    return resource_room_contact_capture(room,source_file,music_command);
}
bool reception_background_contact_return(unsigned *scene_key){
    if(!room || !scene_key || contact_returning || !room->contact_valid || reception_background_busy())return false;
    published_viewport();
    unsigned key=(unsigned)room->contact.bytes[0]<<8|room->contact.bytes[1];
    if(!reception_background_supports_scene(key>>8,key&255) ||
       !resource_room_contact_return(room,source_file) ||
       !music_player_room_pcm_set_attenuation(room->pcm_suppression))return false;
    request_viewport(key);
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
size_t reception_background_checkpoint_bytes(void){return resource_room_checkpoint_bytes(room);}
bool reception_background_checkpoint_export_blocks(uint8_t *const blocks[],size_t block_bytes,size_t bytes){
    return resource_room_checkpoint_export_blocks(room,source_file,popup_active,
        music_player_room_pcm_bank(),blocks,block_bytes,bytes);
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
       !reception_background_rebind()){
        reception_background_end();return false;
    }
    /* save_restore_native still owns its complete temporary snapshot here.
     * The caller releases it before the shared PCM/FM allocations below. */
    checkpoint_pcm=pcm;checkpoint_audio_pending=true;popup_active=popup;return true;
}
bool reception_background_checkpoint_resume_audio(void){
    if(!room || !checkpoint_audio_pending)return false;
    unsigned pcm=checkpoint_pcm;
    if((pcm && music_player_room_pcm_load(pcm)!=1) ||
       !music_player_room_pcm_set_attenuation(room->pcm_suppression)){
        reception_background_end();return false;
    }
    checkpoint_audio_pending=false;room_bind_devices();
    origin=vblanks;ticked=0;reception_generation++;return true;
}
bool reception_background_busy(void){return room && (room->tasks.active || room->queued || practice_reset);}
bool reception_background_rebind(void){
    reception_background_invalidate_picture();
    return room && resource_background_bind_vram(room->video.front_vram,room->video.front_palette,32,64,0xe000,room->video.plane_b,0,0,256,224);
}
bool reception_background_adopt_entry(void){
    if(!native_entry_take_initial_room(&room,&bank,&sub))return false;
    source_file=8;reception_source_scene=0x0802;popup_active=false;reception_source_epoch++;
    viewport_key=reception_source_scene;viewport_pending=false;
    room->current_pcm=music_player_room_pcm_id();
    room->pcm_suppression=music_player_room_pcm_attenuation();
    room->video.scene.resource=bank;room->video.scene.subcode=sub;
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
volatile unsigned reception_work_phase,room_tick_debt,room_tick_batch,room_tick_max_batch;
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
    /* Ordinary rooms share the same foreground CPU as speech and FM.
     * Preserve the entire source backlog, but return after two ticks so
     * sound refills, speech timers and input can run before the next batch.
     * Never rebase origin/ticked or discard an original task publication. */
    unsigned budget=2u,started=ticked;
    while(ticked<target && budget--){
        /* Original SUB D5FE resets the relative clock even when no disc
         * track is playing; never retain a completed track across scenes. */
        room->io.cd_status=music_player_story_cd_status();
        resource_room_cd_clock(room,music_player_story_cd_clock());
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
    room_tick_batch=ticked-started;
    room_tick_debt=target>ticked?target-ticked:0;
    if(room_tick_batch>room_tick_max_batch)room_tick_max_batch=room_tick_batch;
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
    if(reception_background_fullframe()){
        bool good=resource_opening_draw_view_indexed_rgb555_cached(&room->video,output,room->presentation.colors,0,224,&retained_picture,copied);
        if(good)reception_draw_frame=room->video.frames;return good;
    }
    if(reception_background_cinematic(NULL,NULL,NULL)){
        unsigned x,w,rows;reception_background_cinematic(&x,&w,&rows);
        bool changed=false;
        if(!resource_opening_draw_view_indexed_rgb555_cached(&room->video,output,room->presentation.colors,cinematic_source_y(),192,&retained_picture,&changed))return false;
        if(changed){
            for(unsigned y=0;y<rows;y++){
                memset(output+y*256,0,x*2);memset(output+y*256+x+w,0,(256-x-w)*2);
            }
            memset(output+rows*256,0,(192-rows)*256*2);
        }
        if(copied)*copied=changed;reception_draw_frame=room->video.frames;return true;
    }
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
    unsigned x,w,rows;
    bool cinematic=reception_background_cinematic(&x,&w,&rows);
    bool good=reception_background_fullframe()?
        resource_opening_draw_full_indices_cached(&room->video,output,palette,&retained_picture,copied):cinematic?
        resource_opening_draw_cinematic_indices(&room->video,output,palette,&retained_picture,copied,x,w,rows,cinematic_source_y()):
        reception_background_terminal()?
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
    story_set_random_state(story_random_state());
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

bool reception_background_practice_delay(unsigned value){
    if(!room || source_file!=14 || room->scene!=0 || value>65535 || practice_prepared || practice_running)return false;
    room->combat_delay=value;return true;
}
bool reception_background_practice_begin(void){
    return reception_background_native_combat_begin(0);
}
bool reception_background_native_combat_begin(unsigned stage){
    if(practice_reset)return false;
    if(!room || !(((stage==0 || stage==11) && source_file==14) || (stage==2 && source_file==5) || (stage==4 && source_file==6) || (stage==3 && source_file==0x17) || ((stage==5 || stage==6) && source_file==0x1d) || (stage==7 && source_file==0x23) || (stage==8 && source_file==0x24) || (stage==9 && source_file==0x0a) || (stage==10 && source_file==0x14) || (stage>=12 && stage<=14 && source_file==0x29)) ||
       !resource_native_combat_room_begin(&practice,room,stage))return false;
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
    if(!room || !((source_file==14 && room->scene==0) || (source_file==5 && room->scene==7) || (source_file==6 && room->scene==1) || (source_file==0x17 && room->scene==2) || (source_file==0x1d && (room->scene==2 || room->scene==3)) || (source_file==0x23 && room->scene==0) || (source_file==0x24 && room->scene==5) || (source_file==0x0a && room->scene==2) || (source_file==0x14 && room->scene==0) || (source_file==0x29 && room->scene>=1 && room->scene<=3)) ||
       !resource_opening_draw_practice_indices(&room->video,scratch,room->presentation.colors,&retained_picture,&copied))return false;
    /* The source lease includes both pixel and palette changes. Only the
     * combat caller retains its completed output; all other callers force
     * a rewrite because transition/UI buffers have independent owners. */
    bool update=force || copied || !practice_output_valid || practice_output!=output || practice_output_warm!=warm;
    bool actual=false;
    memset(practice_dirty_rows,0,192);
    if(update){
        uint16_t palette[64];
        for(unsigned i=0;i<64;i++)palette[i]=warm?video_filters_warm(room->presentation.colors[i]):room->presentation.colors[i];
        bool full=force || !practice_output_valid || practice_output!=output || practice_output_warm!=warm;
        actual=practice_expand_rgb555_changed_rows(scratch,palette,output,full,resource_background_last_changed_rows());
        practice_output=output;practice_output_warm=warm;practice_output_valid=true;
    }
    if(changed)*changed=actual;
    reception_draw_frame=room->video.frames;return true;
}
int reception_background_practice_touch(int x,int y){
    if(x<0 || x>=256 || y<0 || y>=192)return -1;
    unsigned sx=16+(unsigned)x*224/256,sy=8+(unsigned)y*128/192;
    return (sx<96?0:sx<160?1:2)+(sy<48?0:sy<88?3:6);
}


bool reception_background_update(uint8_t pcm_flags){
    reception_work_phase=1;
    unsigned previous=system_perf_enter(PERF_SCENE);
    bool good=reception_background_update_inner(pcm_flags);
    system_perf_leave(previous);
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

bool reception_background_cd_begin(unsigned track){return room && resource_room_cd_begin(room,track);}
bool reception_background_cd_wait(void){return room && resource_room_cd_wait(room);}

bool reception_background_cd_time(unsigned m,unsigned s,unsigned f){return room && resource_room_cd_time(room,m,s,f);}

bool reception_background_cinematic(unsigned *x,unsigned *width,unsigned *rows){
    /* Only integral cards use a taller/wider viewport. H15 city shots,
     * H09 illustrations and S2 phone/flight shots use the common y8 picture
     * window; the original y149 face actors belong on the lower LCD. */
    if(!room)return false;
    unsigned key=published_viewport(),file=key>>8,scene=key&255;
    if(wide_picture() || (file==0x33 && scene<=3)){
        if(x)*x=0;
        if(width)*width=256;
        if(rows)*rows=file==0x33?176:128;
        return true;
    }
    bool report=(file==7 && scene==14) || (file==17 && scene==3);
    bool title=file==22 && (scene==1 || scene==2);
    bool end=(file==6 && scene==10) || (file==21 && scene==5);
    bool backup=file==4 && (scene==5 || scene==6);
    if(!report && !title && !end && !backup)return false;
    if(x)*x=report?0:32;
    if(width)*width=report?256:192;
    if(rows)*rows=160;
    return true;
}

bool reception_background_flashlight_mode(unsigned mode){
    return source_file==29 && resource_room_flashlight_mode(room,mode);
}
bool reception_background_flashlight_active(void){return room && source_file==29 && room->menu_mode==1;}
bool reception_background_flashlight_target(unsigned *value){
    if(!value || !reception_background_flashlight_active() || !resource_room_flashlight_input(room,0,0))return false;
    *value=(unsigned)room->flashlight[12]<<8|room->flashlight[13];return true;
}
bool reception_background_flashlight_touch(int x,int y,bool held){
    return reception_background_flashlight_active() && resource_room_flashlight_touch(room,x,y) &&
        resource_room_flashlight_input(room,held?16:0,0);
}
bool reception_background_flashlight_position(int *x,int *y){
    if(!x || !y || !reception_background_flashlight_active())return false;
    *x=(int16_t)((unsigned)room->flashlight[0]<<8|room->flashlight[1]);
    *y=(int16_t)((unsigned)room->flashlight[4]<<8|room->flashlight[5]);return true;
}

unsigned reception_background_picture_rows(void){
    unsigned rows=128;
    if(reception_background_fullframe())return 192;
    if(reception_background_cinematic(NULL,NULL,&rows))return rows;
    return reception_background_terminal()?144:128;
}
