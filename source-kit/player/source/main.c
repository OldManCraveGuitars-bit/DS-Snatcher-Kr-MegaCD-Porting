#include "system_perf.h"
extern void runtime_failure_record(unsigned,unsigned,unsigned);
extern void runtime_failure_reset(void);
#include "combat_damage.h"
#include "battle_feedback.h"
#include "resource_portrait.h"
#include <stdio.h>
extern size_t audio_serviced_fread(void *,size_t,size_t,FILE *);
#include <nds.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <filesystem.h>
#include "../generated/text.h"
#include "../generated/voice_audio_r22.h"
#include "../generated/office_images.h"
#include "ui_theme.h"
#include "../generated/office_branch.h"
#include "office_vm.h"
#include "save_game.h"
#include "chapter_progress.h"
#include "opening_player.h"
#include "portrait_motion.h"
#include "office_background.h"
#include "story_vm.h"
#include "../generated/flashlight_touch.h"
#include "story_text.h"
#include "name_keyboard.h"
#include "../generated/people_keyboard.h"
#include "story_audio.h"
#include "../generated/contact_audio.h"
#include "settings.h"
#include "../generated/audio_options.h"
#include "reception_background.h"
#include "save_summary.h"
#include "hq_scene.h"
#include "battle_slide.h"
#include "choice_layout.h"
#include "combat.h"
#include "../generated/battle_rules_r21.h"
#include "../generated/grant_audio.h"
#include "native_startup.h"
#include "native_credits.h"
#include "../generated/ending_text.h"
#include "music_player.h"
extern bool music_player_update_slice(void);
extern bool music_player_slice_active(void);
#include "native_entry_display.h"
#include "native_entry.h"
#include "upper_backdrop.h"
#include "upper_layout.h"
#include "video_filters.h"

static inline void picture_dma_copy(const void *source,void *dest,unsigned bytes){
    if(((uintptr_t)source|(uintptr_t)dest|bytes)&3)dmaCopy(source,dest,bytes);
    else dmaCopyWords(3,source,dest,bytes);
}
static uint16_t top[256*192] __attribute__((aligned(4)));
static uint16_t bottom[256*192] __attribute__((aligned(4)));
static uint16_t *top_vram, *bottom_vram;
/* Native combat already supplies colour indices. DS affine hardware exactly
 * reproduces floor(x*224/256), floor(y*128/192) on this192-row viewport:
 * floor(y*171/256)==floor(y*2/3) for every y in0..191. */
static bool bottom_combat_indexed,bottom_combat_rows_partial;
static bool bottom_combat_rows[128];
static uint16_t bottom_combat_palette[64];
static void lower_bitmap_mode(bool indexed){
    if(((REG_BG3CNT_SUB&BIT(2))!=0)!=!indexed){
        int bg=bgInitSub(3,indexed?BgType_Bmp8:BgType_Bmp16,
            indexed?BgSize_B8_256x256:BgSize_B16_256x256,0,0);
        bgSetPriority(bg,0);bgWrapOff(bg);
        bgSetScale(bg,indexed?224:256,indexed?171:256);
        bgSetScrollf(bg,indexed?16*256:0,0);bgUpdate();
    }
    if(indexed)for(unsigned i=0;i<64;i++)BG_PALETTE_SUB[128+i]=bottom_combat_palette[i]&32767;
}
static void lower_combat_transfer(bool flush){
    const uint8_t *pixels=(const uint8_t*)bottom;
    if(!flush)lower_bitmap_mode(true);
    for(unsigned y=0;y<128;){
        if(bottom_combat_rows_partial && !bottom_combat_rows[y]){y++;continue;}
        unsigned first=y++;
        while(y<128 && (!bottom_combat_rows_partial || bottom_combat_rows[y]))y++;
        unsigned at=first*256,bytes=(y-first)*256;
        if(flush)DC_FlushRange(pixels+at,bytes);
        else picture_dma_copy(pixels+at,(uint8_t*)bottom_vram+at,bytes);
    }
}
/* True only when top contains original colour numbers, not RGB pixels. */
static bool top_room_indexed;
static uint16_t top_room_palette[64];
static bool top_room_palette_pending;
static bool top_room_rows_partial;
static bool top_room_rows[192];
/* A row lease is valid only for this retained destination and publication.
 * New scenes, dimensions, non-room draws and external owners take full DMA. */
static void top_room_transfer(bool flush){
    const uint8_t *pixels=(const uint8_t*)top;
    if(!top_room_rows_partial){
        if(flush)DC_FlushRange(pixels,256*192);
        else picture_dma_copy(pixels,top_vram,256*192);
        return;
    }
    for(unsigned y=0;y<192;){
        if(!top_room_rows[y]){y++;continue;}
        unsigned first=y;while(y<192 && top_room_rows[y])y++;
        if(flush)DC_FlushRange(pixels+first*256,(y-first)*256);
        else picture_dma_copy(pixels+first*256,(uint8_t*)top_vram+first*256,(y-first)*256);
    }
}
static unsigned top_room_fade=256;
static void top_room_expand(void){
    if(!top_room_indexed)return;
    const uint8_t *indices=(const uint8_t*)top;
    /* Reverse order permits safe in-place expansion for old RGB consumers. */
    for(unsigned i=256*192;i-->0;)top[i]=indices[i]?(top_room_palette[indices[i]&63]|0x8000):0;
    top_room_indexed=false;reception_background_invalidate_picture();
}
static void upper_bitmap_mode(bool indexed){
    top_room_palette_pending=false;
    if(((REG_BG3CNT&BIT(2))!=0)!=!indexed){
        int bg=bgInitHidden(3,indexed?BgType_Bmp8:BgType_Bmp16,
            indexed?BgSize_B8_256x256:BgSize_B16_256x256,0,0);
        bgSetPriority(bg,0);
        dmaFillWords(0,(uint8_t*)BG_GFX+256*192*(indexed?1:2),256*64*(indexed?1:2));
        bgShow(bg);
    }
    if(indexed)for(unsigned i=0;i<64;i++){
        unsigned color=top_room_palette[i&63];
        color=(settings_tv_filter?video_filters_warm(color):color)&32767;
        if(top_room_fade<256){
            unsigned r=((color&31)*top_room_fade+128)>>8,g=(((color>>5)&31)*top_room_fade+128)>>8,b=(((color>>10)&31)*top_room_fade+128)>>8;
            color=r|(g<<5)|(b<<10);
        }
        BG_PALETTE[128+i]=color;BG_PALETTE[192+i]=color;
    }
}
#ifdef SNATCHER_OPENING_PROFILE
volatile uint32_t opening_loop_profile[32][16];
extern volatile uint32_t opening_audio_profile[3];
static unsigned opening_loop_profile_index;
#endif
#ifdef SNATCHER_ENTRY_PROFILE
/* Diagnostic-only timings. No frame/script/input state is injected. */
volatile uint32_t factory_loop_profile[32][16];
volatile unsigned factory_loop_profile_index;
extern volatile unsigned factory_room_tick_cycles,factory_room_draw_cycles;
extern volatile unsigned factory_pcm_render_cycles,music_bus_io_cycles;
#endif
/* Slots4..7 remain reserved so historical save/UI mode IDs stay fixed. */
enum { DIALOGUE, MENU, SELECTED, COMBAT, VOICE_PLAYING, VOICE_GAP, VOICE_TAIL, VOICE_CLEAR, OFFICE_ROOT, OFFICE_MENU, OFFICE_DIALOG, TITLE, SYSTEM, NOTICE, CONFIRM, OPENING, RECEPTION_DIALOG, RECEPTION_ROOT, STORY_DIALOG_UI, STORY_MENU_UI, STORY_VOICE_UI, STORY_WAIT_UI, STORY_GAP_UI, STORY_VOICE_START_UI, BOOT_UI, SPLASH_UI, SETTINGS_UI, CASE_FLIGHT_UI, BATTLE_SLIDE_IN, BATTLE_SLIDE_OUT, STORY_COMBAT_UI, STORY_FAILURE_UI, SLOT_SAVE_UI, SLOT_LOAD_UI, STORY_KEYBOARD_UI, STORY_INPUT_UI, CREDITS_UI, ACT_SELECT_UI };
static NameKeyboard name_keyboard;
_Static_assert(OFFICE_ROOT==8 && OFFICE_DIALOG==10,"Save format mode IDs changed");
_Static_assert(STORY_DIALOG_UI==18 && STORY_MENU_UI==19,"Story save mode IDs changed");
static unsigned frame, hits;
static int mode=TITLE, selected, tx=128, ty=95, flash;
static int voice_page_index;
static int long_menu, scroll, drag_x, drag_start, drag_last, dragged, menu_total;
static int row_top[6],row_height[6],clip_top=0,clip_bottom=192;
static unsigned office_line,office_page;
static int office_error;
static bool chapter_clear_pending,chapter_preroll;
static int resume_mode=OFFICE_ROOT,notice_return=TITLE,confirm_action,opening_to_game,title_selected;
static const uint16_t *notice_text;
static unsigned story_scene,story_page,story_until;
static unsigned story_under_scene;
static bool flight_resource,flight_scene,factory_warning;
static uint16_t hq_pixels[256*192] __attribute__((aligned(4)));
static unsigned hq_cached=65535;
static uint16_t battle_pixels[256*192] __attribute__((aligned(4)));
static unsigned battle_tick;
static int battle_return_mode=MENU;
static bool story_battle_active,story_practice_active,combat_queued_fire;
static unsigned story_native_battle_stage;
static unsigned practice_shots;
static unsigned combat_aim=4;
static uint16_t *combat_sprites,*combat_faces;
static unsigned combat_death_ticks;
static volatile unsigned combat_flash_origin;
static volatile bool combat_has_flash;
volatile unsigned combat_feedback_diagnostic[4];
static void combat_assets_end(void){
    free(combat_sprites);combat_sprites=NULL;free(combat_faces);combat_faces=NULL;combat_has_flash=false;
}
static void *combat_asset(const char *name,unsigned expected,unsigned *size){
    FILE *file=fopen(name,"rb");if(!file)return NULL;
    bool good=!fseek(file,0,SEEK_END);long length=good?ftell(file):-1;
    good=good && length>0 && length<=262144 && !(length&3) && (!expected || (unsigned)length==expected) && !fseek(file,0,SEEK_SET);
    void *data=good?malloc(length):NULL;
    good=data && audio_serviced_fread(data,1,length,file)==(size_t)length && !ferror(file);
    if(fclose(file))good=false;
    if(!good){free(data);return NULL;}
    if(size)*size=(unsigned)length;
    DC_FlushRange(data,length);return data;
}
static bool combat_assets_begin(bool practice){
    combat_assets_end();if(!nitroFSInit(NULL))return false;
    if(!practice){combat_sprites=combat_asset("nitro:/battle-sprites-r21.bin",BATTLE_SPRITE_FRAMES*2048,NULL);
        if(!combat_sprites)return false;}
    unsigned bytes=0;void *faces=combat_asset("nitro:/native-battle-faces-r22.nspr",0,&bytes);
    combat_faces=malloc(4*4096*2);bool good=faces && combat_faces;
    for(unsigned i=0;good && i<4;i++)good=resource_portrait_draw_data(faces,bytes,i,combat_faces+i*4096);
    free(faces);if(!good){combat_assets_end();return false;}
    combat_death_ticks=0;combat_has_flash=false;
    return true;
}
static void begin_battle(void){
    top_room_expand();memcpy(battle_pixels,top,sizeof(battle_pixels));battle_return_mode=mode;
    battle_tick=0;mode=BATTLE_SLIDE_IN;dragged=1;
}
static void finish_battle(void){battle_tick=0;mode=BATTLE_SLIDE_OUT;dragged=1;}
static int story_error,story_selected,story_scroll;
static ChoiceLayout story_choices;
static void theme(uint16_t *dst);
static bool story_native_random(uint32_t *value,bool store){
    return reception_background_random_state(value,store) || native_entry_random_state(value,store);
}
static void story_dispatch(int event);
static void story_next(void);
static void finish_legacy_office(int section);
static int story_legacy_section;
static unsigned system_selected,settings_selected;
static bool settings_general;
static volatile unsigned start_screen_fade;
volatile unsigned display_frames;
static volatile unsigned start_screen_origin;
static unsigned screen_transition_action;
static volatile unsigned screen_transition_phase;
static int screen_transition_back,confirm_back=SYSTEM;
static unsigned counter_entry_fade;
static bool story_room_wait,story_battle_wait;
static bool story_sound_wait,story_pcm_wait;
static unsigned header_focus,slot_selected;
static int slot_back,slot_status[SAVE_SLOT_COUNT];
static SaveSummary slot_summaries[SAVE_SLOT_COUNT];
static bool slot_previews_valid;
static void open_slots(bool writing,int back){
    if(!slot_previews_valid){
        static GameCheckpoint preview;
        for(unsigned i=0;i<SAVE_SLOT_COUNT;i++){
            slot_status[i]=save_peek_slot(i+1,&preview);
            slot_summaries[i]=save_summary(slot_status[i]==SAVE_OK?&preview:NULL);
        }
        slot_previews_valid=true;
    }
    slot_back=back;slot_selected=save_current_slot()-1;mode=writing?SLOT_SAVE_UI:SLOT_LOAD_UI;
}
#define C(r,g,b) (0x8000 | ((r)&31) | (((g)&31)<<5) | (((b)&31)<<10))
static int voice_locked(void) {
    return (mode==CREDITS_UI && !native_credits_waiting()) || (mode==OPENING && opening_act1_title()) || mode==STORY_VOICE_UI || mode==STORY_WAIT_UI || mode==STORY_GAP_UI || mode==STORY_VOICE_START_UI || mode==CASE_FLIGHT_UI || mode==BATTLE_SLIDE_IN || mode==BATTLE_SLIDE_OUT;
}
/* Audio refills may span several display frames. Capture edges at VBlank so
 * a short tap is not lost between main-loop polls. Each edge retains its
 * touch position and UI context; input from a locked/previous screen never
 * advances the next dialogue or activates a newly opened menu. */
typedef struct { int down,held,up,context;touchPosition touch; } InputEdge;
static InputEdge input_edges[32],input_latest;
static volatile unsigned input_read,input_write,input_overflow;
static volatile unsigned combat_present_pending;
volatile unsigned combat_present_count,combat_present_late;
static unsigned combat_present_top,combat_present_bottom,combat_present_first,combat_present_last,combat_present_left=4,combat_present_width=4;
static unsigned audio_present_full,audio_present_upper_bytes,audio_present_lower;
static unsigned audio_present_lower_first,audio_present_lower_bytes;
static unsigned audio_present_slide,audio_present_first,audio_present_last;
static bool slide_draw_partial;
static unsigned slide_draw_old,slide_draw_new;
volatile unsigned bitmap_transfer_profile[8];
static void slide_transfer(unsigned first,unsigned last,bool flush){
    (void)first;(void)last;
    battle_slide_copy spans[3];
    unsigned count=battle_slide_dma_ranges(slide_draw_old,slide_draw_new,spans);
    for(unsigned i=0;i<count;i++){
        const battle_slide_copy *s=spans+i;
        const uint16_t *pixels=(s->source==0?battle_pixels:s->source==1?top:bottom)+s->source_row*256;
        if(flush){
            /* The scene was flushed once on entry and stays immutable.
             * Only newly revealed casing rows have changed in RAM. */
            if(s->source)DC_FlushRange(pixels,s->rows*512);
        }else picture_dma_copy(pixels,(s->screen?bottom_vram:top_vram)+s->destination_row*256,s->rows*512);
    }
}

static const uint16_t *audio_present_pixels;
static void fit_upper_picture(void);
static void combat_present_vblank(void);
static bool gameover_visible(void){
    /* Authored retry branches share the complete Y05 game-over raster.
     * Select by artwork identity: later modules use the same scene too. */
    if(story_scene==HQ_SCENE(0x2905)){
        if(mode==STORY_DIALOG_UI || mode==STORY_MENU_UI || mode==STORY_WAIT_UI)return true;
        if((mode==SYSTEM || mode==SETTINGS_UI || mode==NOTICE || mode==CONFIRM || mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI) &&
           (resume_mode==STORY_DIALOG_UI || resume_mode==STORY_MENU_UI))return true;
    }
    return mode==STORY_FAILURE_UI || (mode==CONFIRM && confirm_back==STORY_FAILURE_UI) ||
        (mode==SLOT_LOAD_UI && slot_back==STORY_FAILURE_UI) ||
        (mode==NOTICE && notice_return==SLOT_LOAD_UI && slot_back==STORY_FAILURE_UI);
}
static bool upper_game_picture(void){
    if(mode==STORY_COMBAT_UI || mode==COMBAT || mode==BATTLE_SLIDE_IN || mode==BATTLE_SLIDE_OUT)return false;
    if(gameover_visible() || mode==CREDITS_UI || mode==BOOT_UI || mode==OPENING || mode==CASE_FLIGHT_UI || mode==TITLE || mode==ACT_SELECT_UI || mode==SPLASH_UI || opening_native_display())return true;
    if((mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI) && slot_back==TITLE)return true;
    if((mode>=STORY_DIALOG_UI && mode<=STORY_VOICE_START_UI) || mode==STORY_KEYBOARD_UI || mode==STORY_INPUT_UI)return true;
    if((mode==SYSTEM || mode==SETTINGS_UI || mode==NOTICE || mode==CONFIRM || mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI) &&
       (resume_mode==STORY_DIALOG_UI || resume_mode==STORY_MENU_UI))return true;
    return mode==RECEPTION_DIALOG || mode==RECEPTION_ROOT || (mode<TITLE && mode!=DIALOGUE);
}
/* Resource loading and the first composition may span several VBlanks.
 * Phase1 holds both LCDs black until the complete first upload has finished.
 * Only this VBlank handler changes transition brightness, never mid-scanout. */
static void screen_fade_vblank(void){
    if(!start_screen_fade)return;
    unsigned tick=display_frames-start_screen_origin;
    unsigned darkness=16;
    if(screen_transition_phase==0)darkness=tick<16?tick:16;
    else if(screen_transition_phase==2)darkness=tick<16?16-tick:0;
    setBrightness(3,-(int)darkness);
}
extern void port_stream_maintenance(void);
static void game_vblank(void){
    unsigned perf_previous=system_perf_enter(PERF_VBLANK);
    port_stream_maintenance();
    display_frames++;
    reception_background_vblank();
    scanKeys();
    InputEdge e={keysDown(),keysHeld(),keysUp(),mode,{0}};
    touchRead(&e.touch);input_latest=e;
    if((e.down || e.up) && !voice_locked() && !start_screen_fade){
        unsigned next=(input_write+1)%32;
        if(next!=input_read){input_edges[input_write]=e;input_write=next;}
        else input_overflow++;
    }
    native_entry_display_vblank();
    video_filters_practice(story_practice_active && mode==STORY_COMBAT_UI);
    video_filters_vblank(upper_game_picture(),
        mode==STORY_COMBAT_UI);
    combat_present_vblank();
    /* Upper-only shot overlay is independent of lower scanline/TV DMA. */
    unsigned strength=mode==STORY_COMBAT_UI && combat_has_flash?battle_feedback_flash(display_frames-combat_flash_origin):0;
    combat_feedback_diagnostic[0]=strength;
    if(strength){REG_BLDCNT=BLEND_FADE_WHITE|BLEND_SRC_BG3|BLEND_SRC_BG2;REG_BLDY=strength;}
    screen_fade_vblank();
    system_perf_leave(perf_previous);
}
static InputEdge poll_input(void){
    int irq=enterCriticalSection();
    InputEdge e=input_latest;e.down=e.up=0;
    while(input_read!=input_write){
        InputEdge pending=input_edges[input_read];input_read=(input_read+1)%32;
        if(pending.context==mode){e=pending;break;}
    }
    leaveCriticalSection(irq);return e;
}

static void replay_voice_scene(void) {
    /* Legacy replay UI rejoins the actual original chief introduction.
     * Scene0A01, actor17, PCM86..94 and every wait come from SP06/06A0.
     * No sampled pose schedule, clip gap, tail or menu-clear delay is added. */
    uint8_t fields[132];office_export(fields);
    /* Replaying is a new room entry, not a repeated original scene request. */
    story_audio_end();reception_background_end();portrait_live_mode(false);story_reset();
    story_legacy_section=story_error=story_page=story_selected=story_scroll=0;
    header_focus=0;story_pcm_wait=story_room_wait=story_sound_wait=story_battle_wait=false;
    story_scene=story_under_scene=3;portrait_live_mode(true);
    story_dispatch(story_start_at(0x6a0,fields));
}
static int saveable(void){return (mode>=OFFICE_ROOT && mode<=OFFICE_DIALOG && !office_error) || ((mode==STORY_DIALOG_UI || mode==STORY_MENU_UI) && !story_error);}
static bool story_room_visible(void);
static int capture_native_room(SaveNativeBuffer *buffer){
    if((resume_mode!=STORY_DIALOG_UI && resume_mode!=STORY_MENU_UI) || !story_room_visible())return 0;
    size_t bytes=reception_background_checkpoint_bytes();
    if(!save_native_buffer_allocate(buffer,bytes))return -1;
    return reception_background_checkpoint_export_blocks(buffer->blocks,SAVE_NATIVE_BLOCK_BYTES,bytes)?1:-1;
}
static void notice(const uint16_t *message,int back){if(message==UI_SYS_OPENING_ERROR)runtime_failure_record(1,mode,story_scene);notice_text=message;notice_return=back;mode=NOTICE;}
static const uint16_t *storage_message(int error,int writing){
    if(error==SAVE_NO_SD)return UI_SYS_NO_SD;
    if(error==SAVE_MISSING)return UI_SYS_NO_SAVE;
    return writing?UI_SYS_SAVE_ERROR:UI_SYS_LOAD_ERROR;
}
static void case_backup_refresh(void);
static void capture_checkpoint(GameCheckpoint *s){
    memset(s,0,sizeof(*s));office_export(s->state);
    if(resume_mode==STORY_DIALOG_UI || resume_mode==STORY_MENU_UI){
        s->mode=resume_mode;s->line=story_scene;s->page=story_page;
        story_export_save(s->story);story_export_fields(s->state);return;
    }
    if(resume_mode==OPENING){s->mode=OPENING;s->line=1;return;}
    if(resume_mode==RECEPTION_DIALOG || resume_mode==RECEPTION_ROOT){s->mode=resume_mode;s->line=resume_mode==RECEPTION_DIALOG?office_line:0;return;}
    s->count=office_queue_count;s->mode=resume_mode;s->line=office_line;s->page=office_page;
    for(unsigned i=0;i<office_queue_count;i++){s->queue[i]=office_queue[i];s->speakers[i]=office_speakers[i];}
}
static void resume_game(void){
    if(resume_mode==OPENING){
        if(!opening_begin(1)){notice(UI_SYS_OPENING_ERROR,TITLE);return;}
    }
    mode=resume_mode;
}
static void load_checkpoint(int back){
    runtime_failure_reset();
    /* Scene reconstruction and Maxmod's first fill run synchronously below
     * this call. A full story checkpoint must not consume their ARM9 stack. */
    static GameCheckpoint s;int error=save_load(&s);
    if(error){notice(storage_message(error,0),back);return;}
    opening_end();music_player_end();music_command=0;reception_background_end();counter_entry_fade=0;
    office_import(s.state);office_queue_count=s.count;office_line=s.line;office_page=s.page;
    for(unsigned i=0;i<s.count;i++){office_queue[i]=s.queue[i];office_speakers[i]=s.speakers[i];}
    office_error=0;mode=s.mode;voice_page_index=0;
    story_error=story_selected=story_scroll=0;header_focus=0;story_pcm_wait=story_room_wait=story_sound_wait=false;
    flight_resource=flight_scene=factory_warning=false;
    if(mode==STORY_DIALOG_UI || mode==STORY_MENU_UI){
        case_backup_refresh();
        if(!story_import(s.story)){notice(UI_SYS_LOAD_ERROR,back);return;}
        story_scene=s.line;story_page=s.page;
        story_under_scene=hq_scene_under(story_scene);
        if((story_scene==4 || story_scene>=0x8000) && story_display_under()!=0xffff){
            if(!hq_scene_valid(story_display_under())){notice(UI_SYS_LOAD_ERROR,back);return;}
            story_under_scene=story_display_under();
        }
        int native=save_restore_native();
        if(native!=SAVE_OK && native!=SAVE_MISSING){notice(UI_SYS_LOAD_ERROR,back);return;}
        bool native_restored=native==SAVE_OK;
        if(native_restored && !reception_background_checkpoint_resume_audio()){notice(UI_SYS_LOAD_ERROR,back);return;}
        if(!native_restored && story_under_scene<=1 && !reception_background_restore(story_under_scene==1?1:2)){
            notice(UI_SYS_LOAD_ERROR,back);return;
        }
        if(!native_restored && (story_under_scene==2 || story_under_scene==3) &&
           !reception_background_restore_source(story_under_scene==2?9:10,0)){
            notice(UI_SYS_LOAD_ERROR,back);return;
        }
        if(!native_restored && story_under_scene>4 && story_under_scene<0x8000 && reception_background_supports_scene((story_under_scene&0x3fff)>>8,story_under_scene&255) &&
           !reception_background_restore_source((story_under_scene&0x3fff)>>8,story_under_scene&255)){
            notice(UI_SYS_LOAD_ERROR,back);return;
        }
        /* SP07/0459 resumes K1 actor28 with PCM5 only while the original
         * motion-detector flag2D10 is set. Reconstruct that caller on load,
         * instead of dropping its native actor and resident effect bank. */
        if(!native_restored && story_under_scene==HQ_SCENE(0x1900) && story_read_field(0x2d10) &&
           (music_player_room_pcm_load(5)!=1 || (!native_restored && !reception_background_control(28,0)))){
            notice(UI_SYS_LOAD_ERROR,back);return;
        }
        if(story_under_scene==HQ_SCENE(0x1905) && !opening_forecourt_restore()){
            notice(UI_SYS_LOAD_ERROR,back);return;
        }
        if(!native_restored && (story_scene==4 || story_scene>=0x8000)){
            unsigned popup=story_scene==4?0x0905:story_scene&0x3fff;
            bool restored=opening_native_display()?
                native_entry_forecourt_popup(popup>>8,popup&255):
                reception_background_restore_popup(popup>>8,popup&255);
            if(!restored){notice(UI_SYS_LOAD_ERROR,back);return;}
        }
        /* A cold process has no previous portrait owner. Restoring the
         * native room must also select its original lower-portrait path. */
        portrait_live_mode(reception_background_active() || native_entry_external_voice_ready());
        /* New checkpoints retain the actual source0A cue, never infer a
         * track from a picture. Older checkpoint music remains unknown. */
        /* Native prefix restoration already executes its original cue. Do not
         * send the saved marker a second time; an actual script0A request
         * still reaches the driver even when its command repeats. */
        if(story_music_request()!=0xffff && story_music_request()!=music_command)
            (void)music_player_command(story_music_request());
        if(mode==STORY_MENU_UI && !reception_background_menu_return()){
            notice(UI_SYS_LOAD_ERROR,back);return;
        }
    }else if(mode==RECEPTION_DIALOG || mode==RECEPTION_ROOT){
        /* The legacy story helper consumes its initial graphics events.
         * Recreate the original counter first so speech owns live actors. */
        if(!reception_background_restore(2)){notice(UI_SYS_LOAD_ERROR,back);return;}
        portrait_live_mode(true);
        int old_mode=mode;story_scene=story_page=0;
        int event=story_legacy_dialog(old_mode==RECEPTION_DIALOG?s.line:0);
        if(old_mode==RECEPTION_ROOT)event=story_advance();
        story_dispatch(event);
    }else if(mode==OFFICE_ROOT || mode==OFFICE_MENU){
        if(!reception_background_restore_source(10,0)){notice(UI_SYS_LOAD_ERROR,back);return;}
        portrait_live_mode(true);
        int legacy=mode;story_reset();story_scene=3;story_dispatch(story_start_at(0x797,s.state));
        if(legacy==OFFICE_MENU && mode==STORY_MENU_UI)story_dispatch(story_pick(1));
    }else if(mode==OFFICE_DIALOG){
        story_reset();story_scene=3;story_legacy_section=1;
    }
    if(mode==OPENING){resume_mode=OPENING;opening_to_game=1;resume_game();}
}
static void finish_opening(bool skipped){
    if(skipped && opening_stage==1 && opening_to_game && !opening_entry_completed()){
        if(opening_act1_title())return;
        if(!opening_skip_to_act1())notice(UI_SYS_OPENING_ERROR,TITLE);
        return;
    }
    opening_end();
    if(!opening_stage){
        if(!opening_begin(1)){notice(UI_SYS_OPENING_ERROR,TITLE);return;}
        mode=OPENING;
    }else if(opening_to_game){
        office_queue_count=0;office_line=office_page=0;office_error=0;
        story_reset();story_scene=story_page=0;story_error=0;
        /* The original entry already fades in on natural completion. A skip
         * bypasses it, so fade the complete counter during its source wait. */
        counter_entry_fade=skipped && !opening_entry_completed()?1:0;
        story_dispatch(opening_entry_completed()?story_start_after_entry():story_start());
    }
    else mode=TITLE;
}
static void start_opening(int game){
    runtime_failure_reset();
    counter_entry_fade=0;
    opening_to_game=game;
    if(game){office_reset();story_reset();office_line=office_page=0;office_error=0;}
    if(!opening_begin(0)){notice(UI_SYS_OPENING_ERROR,TITLE);return;}
    mode=OPENING;
}
static void begin_screen_transition(unsigned action,int back);
static unsigned title_choices(void){return chapter_progress_cleared()?3:2;}
static void chapter_record(unsigned chapter,const uint8_t seed[360]){(void)chapter_progress_record(chapter,seed);}
static void start_selected_chapter(unsigned chapter){
    runtime_failure_reset();
    if(!chapter_progress_available(chapter)){notice(UI_SYS_NO_SAVE,ACT_SELECT_UI);return;}
    if(chapter==1){
        opening_to_game=1;
        if(!opening_begin(1) || !opening_skip_to_act1()){notice(UI_SYS_OPENING_ERROR,TITLE);return;}
        mode=OPENING;return;
    }
    uint8_t seed[CHAPTER_SEED_BYTES];
    if(!chapter_progress_read(chapter,seed)){notice(UI_SYS_LOAD_ERROR,ACT_SELECT_UI);return;}
    chapter_preroll=true;
    case_backup_refresh();story_dispatch(story_replay_chapter(chapter,seed));
}
static void chapter_action(unsigned index){
    if(index<3 && chapter_progress_available(index+1))begin_screen_transition(index+3,ACT_SELECT_UI);
}
static void title_action(int choice){
    if(choice==0)start_opening(1);
    else if(choice==1)open_slots(false,TITLE);
    else if(choice==2 && chapter_progress_cleared()){title_selected=0;mode=ACT_SELECT_UI;}
}
static void show_audio_diagnostics(void);
static void system_action(int choice){
    if(choice==0)open_slots(true,SYSTEM);
    else if(choice==1)open_slots(false,SYSTEM);
    else if(choice==2){settings_selected=0;settings_general=false;mode=SETTINGS_UI;}
    else resume_game();
}
extern void audio_trace_sample(unsigned,unsigned,unsigned);
extern void audio_trace_write(void);
extern volatile int audio_trace_write_result;
static uint16_t diagnostic_text[256];
static char diagnostic_message[256];
static void show_audio_diagnostics(void){
    system_perf_summary(diagnostic_message,sizeof(diagnostic_message));
    audio_trace_write();
    size_t used=strlen(diagnostic_message);
    if(!audio_trace_write_result)snprintf(diagnostic_message+used,sizeof(diagnostic_message)-used,"LOG SAVED V101\nSEND THIS SCREEN");
    else snprintf(diagnostic_message+used,sizeof(diagnostic_message)-used,"LOG FAIL %d\nERRNO %u",audio_trace_write_result,system_perf_write_errno);
    unsigned i=0;
    for(;i+1<sizeof(diagnostic_text)/sizeof(*diagnostic_text) && diagnostic_message[i];i++){
        unsigned ch=(unsigned char)diagnostic_message[i];
        diagnostic_text[i]=ch=='\n'?65534:ch<128 && keyboard_ascii[ch]!=65535?keyboard_ascii[ch]:keyboard_ascii['?'];
    }
    diagnostic_text[i]=65535;
    notice(diagnostic_text,SYSTEM);
}
static void open_system(void){resume_mode=mode;system_selected=0;mode=SYSTEM;}
static void begin_screen_transition(unsigned action,int back){
    if(action==1)slot_previews_valid=false;
    int irq=enterCriticalSection();
    start_screen_origin=display_frames;
    screen_transition_action=action;screen_transition_back=back;screen_transition_phase=0;
    start_screen_fade=1;input_read=input_write;leaveCriticalSection(irq);
}
static void confirm_choice(bool yes){
    if(!yes){mode=confirm_back;return;}
    begin_screen_transition(confirm_action==1?2:1,confirm_back);
}
static void slot_action(unsigned index){
    if(index>=SAVE_SLOT_COUNT || !save_select_slot(index+1))return;
    if(mode==SLOT_SAVE_UI){static GameCheckpoint s;capture_checkpoint(&s);int error=save_write(&s);
        /* save_write already reopens and checks the committed bank. Update
         * that slot from the verified checkpoint; the other four have not
         * changed. Rechecking all ten banks here starves a live scene. */
        if(!error){if(s.story[1])story_save_committed();slot_status[index]=SAVE_OK;slot_summaries[index]=save_summary(&s);}
        else slot_previews_valid=false;
        notice(error?storage_message(error,1):UI_SYS_SAVED,SYSTEM);}
    else if(slot_status[index]!=SAVE_OK)notice(storage_message(slot_status[index],0),SLOT_LOAD_UI);
    else begin_screen_transition(2,SLOT_LOAD_UI);
}
static void settings_back(void){
    if(settings_general){settings_general=false;settings_selected=4;}
    else mode=SYSTEM;
}
static void settings_action(unsigned choice){
    if(choice>4)return;
    settings_selected=choice;
    if(choice==4){settings_general=!settings_general;settings_selected=0;return;}
    if(!settings_general){
        if(choice==0)settings_bgm=!settings_bgm;
        else if(choice==1)settings_effects=!settings_effects;
        else if(choice==2)settings_voice=!settings_voice;
        else settings_level=(settings_level+1)%5;
    }else{
        if(choice==0)settings_buttons=!settings_buttons;
        else if(choice==1)settings_scanlines=!settings_scanlines;
        else if(choice==2)settings_tv_filter=!settings_tv_filter;
        else{confirm_action=2;confirm_back=SETTINGS_UI;system_selected=1;mode=CONFIRM;return;}
    }
    if(!settings_store())notice(UI_SYS_SAVE_ERROR,SETTINGS_UI);
}
static void update_start_screen_fade(void){
    if(!start_screen_fade)return;
    unsigned tick=display_frames-start_screen_origin+1;
    if(!screen_transition_phase && tick>=17){
        /* Stay fully black during synchronous resource/audio reconstruction.
         * Start a fresh fade-in clock after loading, however long SD took. */
        /* VBlank has already held black for a complete display. */
        if(screen_transition_action==2)load_checkpoint(screen_transition_back);
        else{
        opening_end();
        music_player_end();
        reception_background_end();
        portrait_motion_end();combat_assets_end();
        office_reset();story_reset();
        story_scene=story_page=story_error=0;story_selected=story_scroll=0;
        counter_entry_fade=0;story_pcm_wait=story_room_wait=story_sound_wait=false;
        office_line=office_page=office_error=0;hq_cached=65535;
        flight_resource=flight_scene=factory_warning=story_battle_active=false;
        opening_to_game=0;chapter_preroll=false;header_focus=title_selected=0;
        mode=SPLASH_UI;
        if(screen_transition_action>=3)start_selected_chapter(screen_transition_action-2);
        else if(screen_transition_back==CREDITS_UI && chapter_clear_pending)notice(UI_SYS_SAVE_ERROR,TITLE);
        }
        screen_transition_phase=1;start_screen_origin=display_frames;return;
    }
    if(screen_transition_phase==2 && tick>17)start_screen_fade=0;
}



static void case_backup_refresh(void){
    static GameCheckpoint state;unsigned occupied=0;
    for(unsigned slot=1;slot<=4;slot++)if(save_peek_slot(slot,&state)==SAVE_OK)occupied|=1u<<(slot-1);
    story_backup_storage(0,occupied);
}
static int case_backup_write(unsigned slot){
    static GameCheckpoint state;memset(&state,0,sizeof(state));
    if(slot<1 || slot>4 || !story_backup_preview(state.story))return SAVE_INVALID;
    state.mode=STORY_DIALOG_UI;state.line=story_scene;
    memcpy(state.state,state.story+24,128);memcpy(state.state+128,state.story+4,4);
    if(!save_select_slot(slot))return SAVE_INVALID;
    int result=save_write(&state);slot_previews_valid=false;return result;
}
static bool flashlight_aim_ui;
static void story_dispatch(int event){
    flashlight_aim_ui=false;

    unsigned budget=128;story_page=0;story_error=0;dragged=1;header_focus=0;
    while(budget--){
        if(event==STORY_INPUT_WAIT){mode=STORY_INPUT_UI;return;}
        if(event==STORY_KEYBOARD){name_keyboard_reset(&name_keyboard);mode=STORY_KEYBOARD_UI;return;}
        if(event==STORY_DIALOG){mode=STORY_DIALOG_UI;return;}
        if(event==STORY_MENU){
            if(!reception_background_menu_return())break;
            story_selected=story_scroll=0;mode=STORY_MENU_UI;return;
        }
        if(event==STORY_BATTLE){
            /* The preceding PCM task is queued by the original script.
             * Let it finish on normal source ticks before task24 takes
             * ownership; do not advance the pending battle event. */
            if((story_event_arg==0 || story_event_arg==2 || story_event_arg==3 || story_event_arg==4 || story_event_arg==5 || story_event_arg==6 || story_event_arg==7 || story_event_arg==8 || story_event_arg==9 || story_event_arg==10 || story_event_arg==11 || story_event_arg==12 || story_event_arg==13 || story_event_arg==14) && reception_background_busy()){
                story_battle_wait=true;mode=STORY_WAIT_UI;return;
            }
            story_battle_wait=false;story_audio_end();
            story_practice_active=story_event_arg==0 || story_event_arg==2 || story_event_arg==3 || story_event_arg==4 || story_event_arg==5 || story_event_arg==6 || story_event_arg==7 || story_event_arg==8 || story_event_arg==9 || story_event_arg==10 || story_event_arg==11 || story_event_arg==12 || story_event_arg==13 || story_event_arg==14;
            story_native_battle_stage=story_event_arg;
            if(story_practice_active){
                if(!combat_assets_begin(true) || !reception_background_native_combat_begin(story_native_battle_stage))break;
                combat_state.damage=0;
                top_room_indexed=false;theme(top);
                if(!reception_background_practice_draw(top,(uint8_t*)hq_pixels))break;
                story_battle_active=true;combat_aim=4;combat_queued_fire=false;practice_shots=0;
                begin_battle();return;
            }
            if(!combat_begin(story_event_arg,story_scene,frame^0x534e4453))break;
            /* SP07/068A selects K1/0. The combat compositor owns a separate
             * 256x192 raster and sprite projection; prepare its original
             * source-decoded backdrop for the cross-screen transition. */
            top_room_indexed=false;reception_background_invalidate_picture();
            if(story_scene==HQ_SCENE(0x1903)){
                if(!reception_background_combat_image(top))break;
            }else if(!hq_scene_read(HQ_SCENE(0x1900),top))break;
            /* The complete source background is now in top. Retire the
             * exploration room before allocating battle sprites and reading
             * the original sound-command bank. Keeping both owners alive
             * exhausted ordinary4MB RAM after loading the warehouse. PCM
             * ownership stays with the existing shared sound device. */
            reception_background_handoff_combat();
            if(!combat_assets_begin(false) || !music_player_combat_begin(story_event_arg))break;
            story_battle_active=true;combat_aim=4;combat_queued_fire=false;begin_battle();return;
        }
        if(event==STORY_GAME_OVER){
            story_audio_end();opening_end();music_player_command(255);
            if(!reception_background_source(0x29,5))break;
            if(!music_player_command(0x5e))break;
            story_scene=HQ_SCENE(0x2905);system_selected=0;mode=STORY_FAILURE_UI;counter_entry_fade=1;return;
        }
        if(event==STORY_VOICE && story_module()==0x40 && story_event_pc==0x4ad && story_event_arg==170){
            if(!opening_return_begin(story_native_fields()))break;
            portrait_live_mode(true);mode=CASE_FLIGHT_UI;return;
        }
        if(event==STORY_VOICE){
            if(!story_event_arg){story_audio_end();event=story_advance();continue;}
            if(reception_background_active()){
                int pcm_kind=music_player_room_pcm_load(story_event_arg);
                if(pcm_kind<0)break;
                if(pcm_kind){
                    story_audio_end();
                    if(!reception_background_voice(story_event_arg))break;
                    event=story_advance();continue;
                }
            }
            if(!story_audio_prepare(story_event_arg))break;
            if(reception_background_active() && !reception_background_voice(story_event_arg))break;
            voice_page_index=0;story_until=frame+1;mode=STORY_VOICE_START_UI;return;
        }
        if(event==STORY_AUDIO_WAIT){
            if(reception_background_active()){
                if(!reception_background_pcm_wait())break;
                story_pcm_wait=true;
                mode=story_audio_running()?STORY_VOICE_UI:STORY_WAIT_UI;return;
            }
            if(story_audio_running()){mode=STORY_VOICE_UI;return;}
            event=story_advance();continue;
        }

        if(event==STORY_BACKUP_CONTEXT){
            /* Port ownership boundary before the shared source routine
             * replaces the picture; this does not run for chapter-local saves. */
            if(!reception_background_contact_capture())break;
            event=story_advance();continue;
        }
        if(event==STORY_BACKUP_QUERY){case_backup_refresh();event=story_advance();continue;}
        if(event==STORY_BACKUP_SAVE){
            int result=case_backup_write(story_event_arg);
            if(!story_backup_finish(result==SAVE_OK?0:255))break;
            event=story_advance();continue;
        }
        if(event==STORY_CREDITS){
            const resource_opening_state *v=reception_background_video();
            uint32_t seed=0;
            if(v)for(unsigned i=0;i<4;i++)seed=(seed<<8)|v->scene.globals[10+i];
            story_audio_end();portrait_motion_end();portrait_live_mode(false);
            top_room_indexed=false;story_room_wait=story_sound_wait=story_pcm_wait=false;
            if(!opening_credits_begin(seed,story_save_counter()))break;
            chapter_clear_pending=true;mode=CREDITS_UI;return;
        }
        if(event==STORY_END){begin_screen_transition(1,mode);return;}
        if(event==STORY_WAIT){story_until=frame+story_event_arg;mode=STORY_WAIT_UI;return;}

        if(event==STORY_CD_TIME){
            if((story_module()!=0x4d && story_module()!=0x58) || !reception_background_cd_time(story_event_arg,story_event_arg2,story_event_arg3))break;
            story_room_wait=true;mode=STORY_WAIT_UI;return;
        }
        if(event==STORY_MEDIA_WAIT){
            if(!((story_module()==0x49 && music_player_story_cd_track()==7) ||
                 (story_module()==0x4d && (music_player_story_cd_track()==8 || music_player_story_cd_track()==9)) ||
                 (story_module()==0x56 && (music_player_story_cd_track()==10 || music_player_story_cd_track()==11)) || (story_module()==0x58 && music_player_story_cd_track()==21) || (story_module()==0x5d && music_player_story_cd_track()==15) || (story_module()==0x5f && music_player_story_cd_track()>=16 && music_player_story_cd_track()<=18) || (story_module()==0x5a && music_player_story_cd_track()>=12 && music_player_story_cd_track()<=14)) ||
               !reception_background_cd_wait())break;
            story_room_wait=true;mode=STORY_WAIT_UI;return;
        }
        if(event==STORY_GRAPHICS){
            // This bounded scene renderer maps the decoded source scene IDs.
            // Resource/transition events remain in the imported program.
            if(story_event_op==0x00 && reception_background_active()){
                if(!reception_background_control(story_event_arg,story_event_arg2))break;
            }else if(story_event_op==0x45 && reception_background_active()){
                if(!reception_background_control(story_event_arg,3))break;
                story_room_wait=true;mode=STORY_WAIT_UI;return;
            }else if(story_event_op==0x41 && story_module()==0x39 && story_event_pc==0xa5 && !story_event_arg){
                if(!reception_background_suppress_actor_effects())break;
            }else if(story_event_op==0x41 && story_event_arg==3){
                if(!reception_background_practice_reset_begin())break;
                story_room_wait=true;mode=STORY_WAIT_UI;return;
            }else if(story_event_op==0x0a){
                if(story_module()==0x3f && story_event_pc==0x1f46 && story_event_arg==255){
                    story_audio_end();
                    if(!opening_begin(3)){opening_end();break;}
                    portrait_live_mode(true);
                    mode=CASE_FLIGHT_UI;return;
                }
                if(!music_player_command(story_event_arg))break;
            }else if(story_event_op==0x0f){
                if(!music_player_effect(story_event_arg))break;
            }else if(story_event_op==0x05){
                if(!reception_background_pcm_command(story_event_arg))break;
            }else if(story_event_op==0x3d){
                if(story_module()==0x40 && story_event_pc==0x74 && story_event_arg==0x1905 && opening_native_display()){
                    if(!opening_forecourt_continue())break;
                    story_scene=HQ_SCENE(0x1905);mode=CASE_FLIGHT_UI;return;
                }
                if(opening_native_display())opening_end();
                unsigned scene=story_event_arg>>8,variant=story_event_arg&255;
                story_set_display_under(0xffff);
                if(reception_background_supports_scene(scene,variant)){if(!reception_background_source(scene,variant))break;}
                else reception_background_end();
                if(scene==8)story_scene=variant==1?1:0;
                else if(scene==9)story_scene=2;
                else if(scene==10 && variant<=1)story_scene=3;
                else if(scene==0x19 && variant<=1)story_scene=HQ_SCENE(0x1900);
                else if(reception_background_supports_scene(scene,variant))story_scene=HQ_SCENE(story_event_arg);
                else if(story_module()==0x40 && story_event_arg==0x1901){
                    // Entry transition uses the same original corridor art.
                    // Large scene object motion remains outside this renderer.
                    story_scene=HQ_SCENE(0x1900);
                }
                else if(story_event_arg==0x2002){if(!flight_resource)break;flight_scene=true;}
                else if(hq_scene_valid(HQ_SCENE(story_event_arg)))story_scene=HQ_SCENE(story_event_arg);
                else break;
                chapter_preroll=false;
                if(story_event_arg==0x2905)counter_entry_fade=1;
            }else if(story_event_op==0x0d){
                if((story_module()==0x49 && story_event_arg==7) ||
                   (story_module()==0x4d && (story_event_arg==8 || story_event_arg==9)) ||
                   (story_module()==0x56 && (story_event_arg==10 || story_event_arg==11)) || (story_module()==0x58 && story_event_arg==21) || (story_module()==0x5d && story_event_arg==15) || (story_module()==0x5f && story_event_arg>=16 && story_event_arg<=18) || (story_module()==0x5a && story_event_arg>=12 && story_event_arg<=14)){
                    if(!reception_background_cd_begin(story_event_arg))break;
                    portrait_live_mode(true);story_room_wait=true;mode=STORY_WAIT_UI;return;
                }
                if(story_module()!=0x3f || story_event_arg!=20)break;
                flight_resource=true;
            }else if(story_event_op==0x42 && story_event_arg==10){
                if(story_module()!=0x53 || !reception_background_flashlight_mode(story_event_arg2))break;
            }else if(story_event_op==0x42 && story_event_arg==11){
                if((story_module()!=0x39 && story_module()!=0x4b && story_module()!=0x5c) || story_event_arg2>1 || (story_module()==0x39 && story_event_arg2!=1))break;
                if(!story_event_arg2){
                    if(!reception_background_contact_capture())break;
                }else{
                    unsigned key,music=reception_background_contact_music();
                    if(music>255 || !reception_background_contact_return(&key) ||
                       !story_set_music_request(music?music:255))break;
                    unsigned file=key>>8,variant=key&255;
                    story_scene=file==8?(variant==1?1:0):file==9?2:file==10 && variant<=1?3:
                        file==0x19 && variant<=1?HQ_SCENE(0x1900):HQ_SCENE(key);
                    story_set_display_under(0xffff);
                    story_room_wait=true;mode=STORY_WAIT_UI;return;
                }
            }else if(story_event_op==0x42 && story_event_arg==7){
                if(story_event_arg2>1)break;
                if(story_module()==0x40)factory_warning=story_event_arg2!=0;
                if(story_module()==0x40 && opening_native_display()){
                    if(!native_entry_pcm_effects_blocked(factory_warning))break;
                }else if(!reception_background_actor_pcm_suppression(story_event_arg2))break;
            }else if(story_event_op==0x42 && story_event_arg==6){
                if(story_module()==0x4e){
                    if(story_event_arg2!=0x78e2 || story_event_arg3!=20 ||
                       !reception_background_practice_delay(story_event_arg3))break;
                }else if(story_module()==0x5a || story_module()==0x5e || story_module()==0x5f){
                    if(story_event_arg2!=0x79ee || !reception_background_display_mode(story_event_arg3))break;
                }else if(story_module()!=0x40)break;
            }else if(story_event_op==0x47 || story_event_op==0x49){
                if(!(story_event_op==0x47?music_player_music_wait_begin():music_player_effect_wait_begin()))break;
                story_sound_wait=true;mode=STORY_WAIT_UI;return;
            }else if(story_event_op==0x42 && story_event_arg==8){
                /* K1/5 keeps its continuous entry renderer and sound owner.
                 * There is deliberately no reception room at this point. */
                bool native_popup=opening_native_display();
                bool was_popup=native_popup?story_scene>=0x8000:reception_background_popup_active();
                if(story_event_arg2){
                    if(!was_popup)story_under_scene=story_scene;
                    story_scene=story_event_arg3==0x0905?4:HQ_OVERLAY(story_event_arg3);
                    if(!hq_scene_valid(story_scene))break;
                    story_set_display_under(story_under_scene);
                    if(!(native_popup?native_entry_forecourt_popup(story_event_arg3>>8,story_event_arg3&255):
                         reception_background_popup(story_event_arg3>>8,story_event_arg3&255)))break;
                }else{
                    if(!(native_popup?native_entry_forecourt_popup_end():reception_background_popup_end()))break;
                    if(was_popup){story_scene=story_under_scene;story_set_display_under(0xffff);}
                }
            }
            event=story_advance();continue;
        }
        if(event==STORY_SCENE && story_event_arg==0x6a0){
            uint8_t fields[132];story_export_fields(fields);office_import(fields);
            office_line=office_page=office_error=0;replay_voice_scene();return;
        }
        break;
    }
    story_audio_end();story_pcm_wait=story_room_wait=story_sound_wait=story_battle_wait=false;story_error=1;mode=STORY_DIALOG_UI;
}
static void keyboard_press(unsigned key){
    unsigned kind=story_keyboard_kind();
    if(key>=name_keyboard_count(kind))return;
    name_keyboard.selected=key;
    if(key==name_keyboard_confirm(kind)){story_dispatch(story_submit_name(name_keyboard.cells));return;}
    if(key==name_keyboard_erase(kind))name_keyboard_delete(&name_keyboard);
    else{
        char ch=name_keyboard_key_for(kind,key);
        if(name_keyboard_character_for(&name_keyboard,kind,ch) && kind==NAME_INPUT_PHONE){
            int actor=name_keyboard_dial_actor(ch);
            if(actor<0 || !reception_background_control((unsigned)actor,0)){
                story_error=1;mode=STORY_DIALOG_UI;return;
            }
        }
    }
    story_generation++;
    if(name_keyboard_ready(&name_keyboard,kind))story_dispatch(story_submit_name(name_keyboard.cells));
}
static void story_next(void){
    if(story_error)return;
    if(++story_page<story_page_count[story_text])return;
    story_dispatch(story_advance());
}
static void finish_legacy_office(int section){
    /* Old queued dialogue still uses its existing text/fields. Before it
     * rejoins the full story, restore original room/portrait command state. */
    if(!reception_background_restore_source(10,0)){notice(UI_SYS_LOAD_ERROR,TITLE);return;}
        portrait_live_mode(true);
    uint8_t fields[132];office_export(fields);story_legacy_section=0;story_scene=3;
    story_dispatch(story_start_at(0x797,fields));
    if(section && mode==STORY_MENU_UI)story_dispatch(story_pick(1));
}
volatile int story_cd_caption=-1;
static void update_story(void){
    /* Fade/load/title ownership suspends source-PC advancement. Continue
     * refilling live speech while the old picture fades; the common main
     * loop still services BGM, native room tasks and VBlank publication. */
    if(start_screen_fade){
        if(story_audio_running() && story_audio_update()<0)story_dispatch(STORY_ERROR);
        return;
    }
    int caption=-1;unsigned played;
    if(music_player_story_cd_track()==7 && music_player_story_cd_sample(&played))
        for(unsigned i=0;i<STORY_CD7_CAPTIONS;i++)
            if(played>=story_cd7_start[i] && played<story_cd7_end[i]){caption=(int)i;break;}
    if(music_player_story_cd_track()>=8 && music_player_story_cd_track()<=9 && music_player_story_cd_sample(&played))
        for(unsigned i=0;i<ACT2_CD_CAPTIONS;i++)
            if(music_player_story_cd_track()==act2_cd_track[i] && played>=act2_cd_start[i] && played<act2_cd_end[i]){caption=STORY_CD7_CAPTIONS+(int)i;break;}
    if(music_player_story_cd_track()>=10 && music_player_story_cd_track()<=11 && music_player_story_cd_sample(&played))
        for(unsigned i=0;i<CRASH_CD_CAPTIONS;i++)
            if(music_player_story_cd_track()==crash_cd_track[i] && played>=crash_cd_start[i] && played<crash_cd_end[i]){caption=STORY_CD7_CAPTIONS+ACT2_CD_CAPTIONS+(int)i;break;}
    if(music_player_story_cd_track()==21 && music_player_story_cd_sample(&played))
        for(unsigned i=0;i<RECONSTRUCT_CD_CAPTIONS;i++)
            if(played>=reconstruct_cd_start[i] && played<reconstruct_cd_end[i]){caption=STORY_CD7_CAPTIONS+ACT2_CD_CAPTIONS+CRASH_CD_CAPTIONS+(int)i;break;}
    if(music_player_story_cd_track()>=12 && music_player_story_cd_track()<=14 && music_player_story_cd_sample(&played))
        for(unsigned i=0;i<SP33_CD_CAPTIONS;i++)
            if(music_player_story_cd_track()==sp33_cd_track[i] && played>=sp33_cd_start[i] && played<sp33_cd_end[i]){caption=STORY_CD7_CAPTIONS+ACT2_CD_CAPTIONS+CRASH_CD_CAPTIONS+RECONSTRUCT_CD_CAPTIONS+(int)i;break;}
    if((music_player_story_cd_track()>=15 && music_player_story_cd_track()<=18) && music_player_story_cd_sample(&played))
        for(unsigned i=0;i<ACT3_CD_CAPTIONS;i++)
            if(music_player_story_cd_track()==act3_cd_track[i] && played>=act3_cd_start[i] && played<act3_cd_end[i]){caption=STORY_CD7_CAPTIONS+ACT2_CD_CAPTIONS+CRASH_CD_CAPTIONS+RECONSTRUCT_CD_CAPTIONS+SP33_CD_CAPTIONS+(int)i;break;}
    if(caption!=story_cd_caption){story_cd_caption=caption;story_generation++;}

    if(mode==STORY_VOICE_START_UI && frame>=story_until){
        if(!story_audio_start()){story_dispatch(STORY_ERROR);return;}
        story_dispatch(story_advance());
    }
    if(story_audio_running()){
        voice_page_index=story_audio_voice==93 && story_audio_samples()>=60549?1:0;
        if(story_audio_voice>=144 && story_audio_voice<=169){
            unsigned voice=story_audio_voice-144,start=combat_story_caption_starts[voice];
            while((unsigned)voice_page_index+1<combat_story_caption_counts[voice] && story_audio_samples()>=combat_story_caption_samples[start+voice_page_index+1])voice_page_index++;
        }
        int contact=contact_voice_index(story_audio_voice);
        if(contact>=0){
            unsigned start=contact_caption_starts[contact];
            while((unsigned)voice_page_index+1<contact_caption_counts[contact] &&
                story_audio_samples()>=contact_caption_samples[start+voice_page_index+1])voice_page_index++;
        }
    }
    int status=story_audio_running()?story_audio_update():0;
    if(status<0){story_dispatch(STORY_ERROR);return;}
    if(story_sound_wait){
        int waiting=music_player_sound_wait_status();
        if(waiting<0){story_dispatch(STORY_ERROR);return;}
        if(waiting)return;
        story_sound_wait=false;story_dispatch(story_advance());return;
    }
    /* An original actor-task wait suspends the story PC, not channel0 or
     * the portrait's PCM clock. Service both before testing task completion. */
    if(story_battle_wait){
        if(reception_background_busy())return;
        story_battle_wait=false;story_dispatch(STORY_BATTLE);return;
    }
    if(story_pcm_wait){
        if(reception_background_busy() || story_audio_running())return;
        story_pcm_wait=false;story_dispatch(story_advance());return;
    }
    if(story_room_wait){
        if(reception_background_busy())return;
        story_room_wait=false;story_dispatch(story_advance());return;
    }
    if(mode==STORY_VOICE_UI){
        if(!status){
            /* Original actor/PCM waits above own room speech timing. The
             * entry player completes on its source clock. Never append a
             * recorded, voice-number-specific delay after either path. */
            story_dispatch(story_advance());
        }
    }else if((mode==STORY_WAIT_UI || mode==STORY_GAP_UI) && frame>=story_until)story_dispatch(story_advance());
}

static void rect(uint16_t *dst,int x,int y,int w,int h,uint16_t color) {
    for(int j=y;j<y+h && j<clip_bottom;j++) for(int i=x;i<x+w && i<256;i++)
        if(i>=0 && j>=clip_top) dst[j*256+i]=color;
}
static void glyph(uint16_t *dst,int x,int y,int id,uint16_t color) {
    for(int j=0;j<12;j++) for(int i=0;i<12;i++)
        if((glyphs[id][j] & (1<<(11-i))) && x+i>=0 && x+i<256 && y+j>=clip_top && y+j<clip_bottom)
            dst[(y+j)*256+x+i]=color;
}
static void text(uint16_t *dst,int x,int y,int width,const uint16_t *s,uint16_t color) {
    int start=x;
    for(;*s!=65535;s++) {
        if(*s==65534){x=start;y+=16;continue;}
        int advance=*s==SPACE_GLYPH?6:12;
        if(x+advance>start+width){x=start;y+=16;}
        if(*s!=SPACE_GLYPH)glyph(dst,x,y,*s,color);
        x+=advance;
    }
}
static int lines(const uint16_t *s) {
    int count=1,x=0;
    for(;*s!=65535;s++) {
        if(*s==65534){count++;x=0;continue;}
        int advance=*s==SPACE_GLYPH?6:12;
        if(x+advance>216){count++;x=0;}x+=advance;
    }
    return count;
}
static int label_width(const uint16_t *s){
    int width=0;
    for(;*s!=65535;s++){
        if(*s==65534)return CHOICE_TEXT_WIDTH+1;
        width+=*s==SPACE_GLYPH?6:12;
    }
    return width;
}
static const uint16_t *story_page_read(unsigned index){
    const uint16_t *p=story_text_page(index);
    if(!p){story_error=true;return UI_OFFICE_ERROR;}
    return p;
}
static void panel(uint16_t *,int,int,int,int);
static void outline(uint16_t *,int,int,int,int);
static bool flashlight_available(void){return story_flashlight_menu() && reception_background_flashlight_active();}
static void draw_flashlight_cursor(void){
    int x=0,y=0;reception_background_flashlight_position(&x,&y);
    x=16+(x+71)*223/139;y=60+(y+39)*119/75;
    int left=x<22?16:x-6,right=x>233?239:x+6,top=y<66?60:y-6,last=y>173?179:y+6;
    rect(bottom,left,y,right-left+1,1,C(8,31,27));rect(bottom,x,top,1,last-top+1,C(8,31,27));
}
static void draw_flashlight(void){
    text(bottom,8,5,96,UI_FLASHLIGHT_LIGHT,C(8,24,27));
    panel(bottom,104,1,48,22);outline(bottom,104,1,48,22);text(bottom,116,6,24,UI_OPTIONS_MENU,C(31,31,31));
    panel(bottom,160,1,88,22);outline(bottom,160,1,88,22);text(bottom,168,6,76,UI_FLASHLIGHT_COMMAND,C(31,31,31));
    text(bottom,16,30,224,UI_FLASHLIGHT_AIM,C(31,31,31));
    panel(bottom,16,60,224,120);outline(bottom,16,60,224,120);
    draw_flashlight_cursor();
}
static void story_layout(void){
    int widths[CHOICE_CAPACITY],full_lines[CHOICE_CAPACITY];
    for(unsigned i=0;i<story_menu_count;i++){
        const uint16_t *s=story_page_read(story_page_start[story_menu_text[i]]);
        widths[i]=label_width(s);full_lines[i]=lines(s);
    }
    choice_layout(&story_choices,story_menu_count,widths,full_lines);
    story_scroll=choice_scroll(&story_choices,story_scroll);
}
static void story_focus(void){
    story_layout();story_scroll=choice_focus(&story_choices,story_selected,story_scroll);
}
static const uint16_t *label(int i) { return long_menu?long_menu_labels[i]:menu_labels[i]; }
static void layout(void) {
    int y=0;
    for(int i=0;i<6;i++){row_top[i]=y;row_height[i]=24+16*(lines(label(i))-1);y+=row_height[i]+2;}
    menu_total=y;
    if(scroll<0)scroll=0;
    int max=menu_total>156?menu_total-156:0;
    if(scroll>max)scroll=max;
}
static void theme(uint16_t *dst) {
    if(dst==top && upper_backdrop_draw(dst))return;
    ui_theme_copy(dst,false,0,256*192);
}
static void panel(uint16_t *dst,int x,int y,int w,int h) {
    // Quiet translucent navy: retain the art beneath selectable controls.
    // Precompute the blend during packing so menu input stays responsive on DS.
    int left=x<0?0:x,right=x+w>256?256:x+w;
    if(left>=right)return;
    int start=y<clip_top?clip_top:y,end=y+h>clip_bottom?clip_bottom:y+h;
    for(int j=start;j<end;j++)ui_theme_copy(dst+j*256+left,true,j*256+left,right-left);
}
static void outline(uint16_t *dst,int x,int y,int w,int h) {
    uint16_t c=C(4,11,13);
    rect(dst,x,y,w,1,c);rect(dst,x,y+h-1,w,1,c);
    rect(dst,x,y,1,h,c);rect(dst,x+w-1,y,1,h,c);
}
static void button(int y,const uint16_t *s) {
    panel(bottom,8,y,240,24);outline(bottom,8,y,240,24);
    text(bottom,20,y+6,216,s,C(28,30,31));
}
static void keyboard_text(int x,int y,const char *s){
    for(;*s;s++,x+=12){unsigned ch=(unsigned char)*s;if(ch<128 && keyboard_ascii[ch]!=65535)glyph(bottom,x,y,keyboard_ascii[ch],C(31,31,31));}
}
static void draw_keyboard(void){
    unsigned kind=story_keyboard_kind();
    unsigned title=kind>=NAME_INPUT_NUMBER?GIBSON_ANSWER_TEXT:kind==NAME_INPUT_PHONE?PHONE_NAME_TEXT:kind==NAME_INPUT_PASSWORD?PASSWORD_NAME_TEXT:PEOPLE_NAME_TEXT;
    text(bottom,8,5,144,story_page_read(story_page_start[title]),C(8,24,27));
    text(bottom,164,5,84,UI_OFFICE_BACK,C(8,24,27));
    panel(bottom,8,25,240,27);outline(bottom,8,25,240,27);
    keyboard_text(24,32,name_keyboard.cells);rect(bottom,24+name_keyboard.cursor*12,46,11,1,C(8,24,27));
    for(unsigned i=0;i<name_keyboard_count(kind);i++){
        int x,y,w,h;name_keyboard_box_for(kind,i,&x,&y,&w,&h);
        panel(bottom,x,y,w,h);outline(bottom,x,y,w,h);
        char letter[2]={name_keyboard_key_for(kind,i),0};const char *label=i==name_keyboard_erase(kind)?"DEL":i==name_keyboard_confirm(kind)?"OK":i==30?"SPACE":letter;
        keyboard_text(x+(w-(int)strlen(label)*12)/2,y+(h-12)/2,label);
        if(i==name_keyboard.selected)rect(bottom,x+1,y+2,2,h-4,C(8,24,27));
    }
}
static void draw_portrait_group(void);
static void portrait(const uint16_t *image,int x) {
    for(int y=0;y<64;y++)memcpy(bottom+(8+y)*256+x,image+y*64,64*2);
}
static void draw_portrait_group(void){
    if(portrait_live_active)
        for(unsigned i=0;i<portrait_live_count();i++)portrait(portrait_live_image(i),portrait_live_x(i));
}


static bool story_room_visible(void){
    if(story_practice_active)return false;
    unsigned scene=reception_background_popup_active()?story_under_scene:story_scene;
    return reception_background_active() && (scene<=1 ||
        (scene==2 && (reception_background_presenting(0x0900) || reception_background_presenting(0x0902))) ||
        (scene==3 && (reception_background_presenting(0x0a00) || reception_background_presenting(0x0a01))) ||
        (scene==HQ_SCENE(0x1900) && (reception_background_presenting(0x1900) || reception_background_presenting(0x1901))) ||
        (scene>4 && scene<0x8000 && reception_background_presenting(scene&0x3fff)));
}
#include "combat_indicator.h"
static bool combat_repaint_top=true,combat_repaint_bottom=true,combat_draw_partial,combat_indicator_only;
static CombatDamageRow combat_rows[192];
static void combat_transfer_rows(bool flush){
    if(bottom_combat_indexed){lower_combat_transfer(flush);return;}
    /* Group consecutive changed rows into one cache/DMA operation. A
     * stationary training fixture need not compete with sound for the bus. */
    if(story_practice_active){
        extern unsigned char practice_dirty_rows[192];
        extern volatile unsigned practice_rows_transferred;
        for(unsigned y=0;y<192;){
            if(!practice_dirty_rows[y]){y++;continue;}
            unsigned first=y;while(y<192 && practice_dirty_rows[y])y++;
            unsigned at=first*256,bytes=(y-first)*512;
            if(flush)DC_FlushRange(bottom+at,bytes);
            else {picture_dma_copy(bottom+at,bottom_vram+at,bytes);practice_rows_transferred+=y-first;}
        }
        return;
    }
    for(unsigned y=0;y<192;y++){
        unsigned left=combat_rows[y].left,right=combat_rows[y].right;
        if(left>=right)continue;
        unsigned at=y*256+left,bytes=(right-left)*2;
        if(flush)DC_FlushRange(bottom+at,bytes);
        else picture_dma_copy(bottom+at,bottom_vram+at,bytes);
    }
}
static void combat_present_vblank(void){
    if(!combat_present_pending)return;
    if(REG_VCOUNT<192)combat_present_late++;
    unsigned transfer_begin=REG_VCOUNT;
    bool full_more=false;
    if(audio_present_slide){lower_bitmap_mode(false);slide_transfer(audio_present_first,audio_present_last,false);}
    else if(audio_present_full){
        if(top_room_indexed && top_room_palette_pending)upper_bitmap_mode(true);
        /* At most the existing one-page DMA budget in one VBlank. */
        bool together=audio_present_upper_bytes+audio_present_lower_bytes<=sizeof(top);
        if(audio_present_upper_bytes){
            upper_bitmap_mode(top_room_indexed);fit_upper_picture();
            if(top_room_indexed && audio_present_pixels==top)top_room_transfer(false);
            else picture_dma_copy(audio_present_pixels,top_vram,audio_present_upper_bytes);
            audio_present_upper_bytes=0;
            if(!together)goto picture_transfer_done;
        }
        if(audio_present_lower){
            unsigned offset=audio_present_lower_first*256;
            if(bottom_combat_indexed)lower_combat_transfer(false);
            else {lower_bitmap_mode(false);picture_dma_copy(bottom+offset,bottom_vram+offset,audio_present_lower_bytes);}
            audio_present_lower=0;
        }
picture_transfer_done:
        full_more=audio_present_upper_bytes || audio_present_lower;
    }else {
    if(combat_present_top)for(unsigned y=combat_present_first;y<combat_present_last;y++)
        picture_dma_copy(top+y*256+combat_present_left,top_vram+y*256+combat_present_left,combat_present_width*2);
    if(combat_present_bottom)combat_transfer_rows(false);
    }
    if(REG_VCOUNT<192)combat_present_late++;
    unsigned transfer_end=REG_VCOUNT;
    unsigned duration=(transfer_end+263-transfer_begin)%263;
    bitmap_transfer_profile[0]=transfer_begin;bitmap_transfer_profile[1]=transfer_end;
    bitmap_transfer_profile[2]+=duration;bitmap_transfer_profile[3]++;
    if(duration>bitmap_transfer_profile[4])bitmap_transfer_profile[4]=duration;
    if(audio_present_slide){bitmap_transfer_profile[5]++;bitmap_transfer_profile[6]+=audio_present_last-audio_present_first;}
    if(transfer_end<192)bitmap_transfer_profile[7]++;
    combat_present_count++;if(!full_more)combat_present_pending=0;
}
static void slide_restore_background(unsigned first,unsigned last){
    unsigned split=last<192?last:192;
    if(first<split && !upper_backdrop_draw_rows(top,first,split))
        ui_theme_copy(top+first*256,false,first*256,(split-first)*256);
    unsigned begin=first>192?first:192;
    if(begin<last)ui_theme_copy(bottom+(begin-192)*256,false,(begin-192)*256,(last-begin)*256);
}
static bool draw_upper_changed,draw_portraits_only;
static bool return_portrait_only(const int now[32],const int previous[32],int previous_stage){
    if(now[0]!=CASE_FLIGHT_UI || opening_stage!=5 || previous_stage!=5)return false;
    /* Only upper native-picture time and lower portrait identity may change.
     * Captions, mode, filters, notices and all other UI fields remain exact. */
    for(unsigned i=0;i<32;i++)if(i!=13 && i!=16 && now[i]!=previous[i])return false;
    return true;
}

static bool draw_room_top(bool *copied){
    bool indexed=!counter_entry_fade;
    if(indexed!=top_room_indexed)reception_background_invalidate_picture();
    top_room_indexed=indexed;
    uint16_t prior_palette[64];
    if(indexed)memcpy(prior_palette,top_room_palette,sizeof(prior_palette));
    bool good=indexed?reception_background_draw_indices((uint8_t*)top,top_room_palette,copied):
        reception_background_draw_cached(top,copied);
    if(!good)top_room_indexed=false;
    else if(indexed && memcmp(prior_palette,top_room_palette,sizeof(prior_palette)))top_room_palette_pending=true;
    const bool *rows=good && indexed && copied && *copied?resource_background_last_changed_rows():NULL;
    if(rows){memcpy(top_room_rows,rows,sizeof(top_room_rows));top_room_rows_partial=true;}
    return good;
}
static bool draw_gameover_top(void){
    if(!top_room_indexed)reception_background_invalidate_picture();
    top_room_indexed=true;top_room_fade=256;
    if(counter_entry_fade){unsigned step=counter_entry_fade-1;top_room_fade=(step*step*(96-2*step)+64)>>7;}
    return reception_background_gameover((uint8_t*)top,top_room_palette);
}
/* Both combat engines publish the same upper status. The lower LCD remains
 * the complete touch shooting field. Stages 0/11 are scored practice; stage14
 * is the final shoot/no-shoot decision, with no player damage controller. */
static void combat_status_draw(void){
    if(combat_indicator_only){combat_indicator_draw(top,(frame/8)%12,false);return;}
    theme(top);combat_indicator_draw(top,(frame/8)%12,true);
    battle_feedback_draw(top,combat_faces,combat_state.damage);
    if(story_native_battle_stage==0 || story_native_battle_stage==11 || story_native_battle_stage==14)return;
    unsigned life=combat_state.damage<6?6-combat_state.damage:0;
    /* Draw after the crack overlay so every remaining/empty cell is legible. */
    /* Face (96,64,64,64), label and 140px meter share the screen centre. */
    panel(top,50,132,156,40);
    text(top,104,136,48,UI_COMBAT_LIFE,C(31,31,31));
    for(unsigned i=0;i<6;i++){
        rect(top,58+24*i,156,20,10,i<life?C(5,12,24):C(7,4,4));
        if(i<life)rect(top,58+24*i,156,20,2,C(12,23,31));
    }
}
static void draw(void) {
    bool indexed=mode==STORY_COMBAT_UI && story_practice_active && !settings_tv_filter && !settings_scanlines;
    if(indexed!=bottom_combat_indexed)reception_background_invalidate_picture();
    bottom_combat_indexed=indexed;
    top_room_fade=256;
    /* General UI/render transitions may reuse top for any artwork. Only
     * the room-only path below owns a retained, unmodified native picture. */
    bool retained_room=story_practice_active || gameover_visible() || story_room_visible() && (((mode>=STORY_DIALOG_UI && mode<=STORY_VOICE_START_UI) || mode==STORY_KEYBOARD_UI || mode==STORY_INPUT_UI) ||
        ((mode==SYSTEM || mode==SETTINGS_UI || mode==NOTICE || mode==CONFIRM || mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI) &&
         (resume_mode==STORY_DIALOG_UI || resume_mode==STORY_MENU_UI)));
    draw_upper_changed=true;
    if(!retained_room){reception_background_invalidate_picture();top_room_indexed=false;}
    if(draw_portraits_only)ui_theme_copy(bottom+8*256,false,8*256,64*256);
    else if(mode!=STORY_COMBAT_UI && !slide_draw_partial)theme(bottom);
    if(mode==BATTLE_SLIDE_IN || mode==BATTLE_SLIDE_OUT){
        if(!slide_draw_partial)theme(top);
        else if(slide_draw_new>slide_draw_old)slide_restore_background(slide_draw_old,slide_draw_new);
        else if(slide_draw_new<slide_draw_old)slide_restore_background(slide_draw_new+192,slide_draw_old+192);
        if(!slide_draw_partial){
            battle_slide_blit(battle_pixels,top,bottom,battle_tick,mode==BATTLE_SLIDE_OUT);
            DC_FlushRange(battle_pixels,sizeof(battle_pixels));
        }
        return;
    }
    if(mode==STORY_COMBAT_UI && story_practice_active){
        if(combat_repaint_top)combat_status_draw();
        if(combat_repaint_bottom){
            bool changed=false;
            if(bottom_combat_indexed){
                uint16_t prior[64];memcpy(prior,bottom_combat_palette,sizeof(prior));
                if(!reception_background_practice_indices((uint8_t*)bottom,bottom_combat_palette,&changed)){story_error=1;return;}
                const bool *rows=changed && combat_draw_partial?resource_background_last_changed_rows():NULL;
                bottom_combat_rows_partial=combat_draw_partial && (!changed || rows);
                if(rows)memcpy(bottom_combat_rows,rows,sizeof(bottom_combat_rows));
                else memset(bottom_combat_rows,changed,sizeof(bottom_combat_rows));
                changed=changed || !combat_draw_partial || memcmp(prior,bottom_combat_palette,sizeof(prior));
            }else{
                if(!combat_draw_partial)theme(bottom);
                if(!reception_background_practice_draw_filtered(bottom,(uint8_t*)hq_pixels,settings_tv_filter,!combat_draw_partial,&changed)){story_error=1;return;}
            }
            combat_repaint_bottom=changed;
        }
        return;
    }
    if(mode==STORY_COMBAT_UI){
        if(combat_repaint_top)combat_status_draw();
        if(!combat_repaint_bottom)return;
        if(combat_draw_partial){
            combat_render_rows(bottom,battle_pixels,combat_sprites,combat_rows);
            if(settings_tv_filter)for(unsigned y=0;y<192;y++)if(combat_rows[y].left<combat_rows[y].right)
                video_filters_copy(bottom+y*256+combat_rows[y].left,bottom+y*256+combat_rows[y].left,combat_rows[y].right-combat_rows[y].left);
        }else{combat_render(bottom,battle_pixels,combat_sprites);if(settings_tv_filter)video_filters_copy(bottom,bottom,256*192);}
        if(combat_state.weapon){
            int x=43+85*(combat_aim%3),y=36+60*(combat_aim/3);
            rect(bottom,x-6,y,13,1,C(8,24,27));rect(bottom,x,y-6,1,13,C(8,24,27));
        }
        return;
    }
    if(mode==BOOT_UI){ /* Media decoder owns the full upper screen. */ }
    else if(mode==CASE_FLIGHT_UI){ /* The flight decoder owns upper artwork. */ }
    else if(mode==TITLE || mode==ACT_SELECT_UI || mode==SPLASH_UI || ((mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI) && slot_back==TITLE)){
        if(!native_startup_title(top))memset(top,0,sizeof(top));
    }
    else if(chapter_preroll){for(unsigned i=0;i<256*192;i++)top[i]=0x8000;}
    else if(gameover_visible()){
        if(!draw_gameover_top()){story_error=1;memset(top,0,sizeof(top));}
    }
    else if(mode==OPENING || mode==CREDITS_UI){ /* Original native player owns upper artwork. */ }
    else if(opening_native_display()){ /* Live scene owns the upper renderer during user input. */ }
    else if(((mode>=STORY_DIALOG_UI && mode<=STORY_VOICE_START_UI) || mode==STORY_KEYBOARD_UI || mode==STORY_INPUT_UI) || ((mode==SYSTEM || mode==SETTINGS_UI || mode==NOTICE || mode==CONFIRM || mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI) && (resume_mode==STORY_DIALOG_UI || resume_mode==STORY_MENU_UI))){
        if(story_room_visible()){
            if(!draw_room_top(&draw_upper_changed)){
                draw_upper_changed=true;story_error=1;mode=STORY_DIALOG_UI;memset(top,0,sizeof(top));
            }
        }else if(story_scene>=0x8000){
            story_error=1;mode=STORY_DIALOG_UI;memset(top,0,sizeof(top));
        }else if(story_scene>4){
            if(hq_cached!=story_scene){
                if(hq_scene_read(story_scene,hq_pixels))hq_cached=story_scene;
                else {story_error=1;mode=STORY_DIALOG_UI;memset(hq_pixels,0,sizeof(hq_pixels));}
            }
            memcpy(top,hq_pixels,sizeof(top));
        }else if(story_scene==3){
            if(!office_background_draw(top)){story_error=1;mode=STORY_DIALOG_UI;memset(top,0,sizeof(top));}
        }else if(story_scene==0 || story_scene==1){
            if(!reception_background_draw(top)){story_error=1;mode=STORY_DIALOG_UI;memset(top,0,sizeof(top));}
        }else {story_error=1;mode=STORY_DIALOG_UI;memset(top,0,sizeof(top));}
    }
    else if(mode==RECEPTION_DIALOG || mode==RECEPTION_ROOT){if(!reception_background_draw(top)){office_error=1;memset(top,0,sizeof(top));}}
    else if(mode>=TITLE)theme(top);
    else if(mode!=COMBAT){
        if(mode==DIALOGUE)theme(top); /* Development text-layout UI. */
        else if(!office_background_draw(top)){office_error=1;mode=OFFICE_DIALOG;memset(top,0,sizeof(top));}
    }
    if(mode==SPLASH_UI){
        panel(bottom,24,80,208,32);outline(bottom,24,80,208,32);
        text(bottom,50,90,168,UI_TOUCH_START,C(28,30,31));
    }else if(mode==BOOT_UI){ /* Keep the quiet lower artwork visible. */ }
    else if(mode==CASE_FLIGHT_UI){
        unsigned samples;
        if(opening_stage==5){
            draw_portrait_group();
            if(draw_portraits_only)return;
            if(opening_caption>=0){
                const uint16_t *caption=opening_caption<11?story_page_read(STORY_CAPTION_BASE_EXIT_VOICE_CAPTIONS+(opening_caption)):story_page_read(STORY_CAPTION_BASE_RETURN_CDDA_CAPTIONS+(opening_caption-11));
                panel(bottom,8,82,240,84);text(bottom,16,88,216,caption,C(31,31,31));
            }
        }else if(opening_flight_voice(&samples)){
            draw_portrait_group();
            panel(bottom,8,82,240,84);text(bottom,16,88,216,story_page_read(STORY_CAPTION_BASE_DISPATCH_CAPTIONS+(8)),C(31,31,31));
        }else if(opening_caption>0 && (unsigned)opening_caption<=STORY_CAPTION_COUNT_FLIGHT_CDDA_CAPTIONS){
            panel(bottom,8,82,240,84);text(bottom,16,88,216,story_page_read(STORY_CAPTION_BASE_FLIGHT_CDDA_CAPTIONS+(opening_caption-1)),C(31,31,31));
        }
    }
    else if(mode==TITLE){
        panel(bottom,8,8,240,36);
        const uint16_t *labels[3]={UI_SYS_NEW,UI_SYS_CONTINUE,UI_ACT_SELECT};
        for(unsigned i=0;i<title_choices();i++){int y=58+36*i;panel(bottom,8,y,240,30);outline(bottom,8,y,240,30);text(bottom,20,y+9,216,labels[i],C(28,30,31));if(i==title_selected)rect(bottom,10,y+5,2,20,C(8,24,27));}
    }else if(mode==ACT_SELECT_UI){
        text(bottom,16,8,140,UI_ACT_SELECT,C(8,24,27));text(bottom,164,8,84,UI_OFFICE_BACK,C(8,24,27));
        const uint16_t *labels[3]={UI_SLOT_ACT1,UI_SLOT_ACT2,UI_SLOT_ACT3};
        for(unsigned i=0;i<3;i++){
            int y=58+36*i;bool available=chapter_progress_available(i+1);
            panel(bottom,8,y,240,30);outline(bottom,8,y,240,30);
            text(bottom,20,y+9,216,labels[i],available?C(28,30,31):C(12,14,15));
            if(i==(unsigned)title_selected)rect(bottom,10,y+5,2,20,C(8,24,27));
        }
    }else if(mode==CREDITS_UI){
        if(opening_caption<=-10){
            unsigned mask=(unsigned)(-10-opening_caption);
            for(unsigned i=0;i<5;i++)if(mask&(1u<<i))text(bottom,16,48+20*i,216,ending_quote_lines[i],C(31,31,31));
        }else if(opening_caption==-3){
            text(bottom,(256-ending_credit_widths[0])/2,80,ending_credit_widths[0],ending_line_5,C(31,31,31));
            text(bottom,(256-ending_credit_widths[1])/2,104,ending_credit_widths[1],ending_line_6,C(31,31,31));
        }else if(opening_caption>=0){
            panel(bottom,8,82,240,84);
            text(bottom,16,88,216,story_page_read(STORY_CAPTION_BASE_ACT3_CD_CAPTIONS+opening_caption),C(31,31,31));
        }
        if(native_credits_waiting())button(166,UI_OFFICE_NEXT);
    }else if(mode==OPENING){
        if(opening_caption>=0){panel(bottom,8,28,240,134);text(bottom,16,36,216,opening_stage?story_page_read(STORY_CAPTION_BASE_TRAVEL_TEXTS+(opening_caption)):story_page_read(STORY_CAPTION_BASE_OPENING_TEXTS+(opening_caption)),C(31,31,31));}
        if(!opening_act1_title())button(166,UI_SYS_SKIP);
    }else if(mode==SYSTEM){
        text(bottom,8,5,216,UI_SYS_TITLE_SYSTEM,C(8,24,27));
        text(bottom,164,5,84,UI_OFFICE_BACK,C(8,24,27));
        const uint16_t *labels[3]={UI_SYS_SAVE,UI_SYS_LOAD,UI_OPTIONS_SETTINGS};
        for(unsigned i=0;i<3;i++){button(30+32*i,labels[i]);if(i==system_selected)rect(bottom,10,35+32*i,2,14,C(8,24,27));}
        if(resume_mode==OPENING)text(bottom,16,160,216,UI_TRAVEL_RESTART,C(8,24,27));
    }else if(mode==SETTINGS_UI){
        text(bottom,8,5,144,settings_general?UI_AUDIO_GENERAL_TITLE:UI_AUDIO_TITLE,C(8,24,27));text(bottom,164,5,84,UI_OFFICE_BACK,C(8,24,27));
        const uint16_t *volumes[5]={UI_OPTIONS_VOLUME_0,UI_OPTIONS_VOLUME_25,UI_OPTIONS_VOLUME_50,UI_OPTIONS_VOLUME_75,UI_OPTIONS_VOLUME_100};
        if(settings_general){
            button(30,settings_buttons?UI_OPTIONS_BUTTONS_ON:UI_OPTIONS_BUTTONS_OFF);
            button(62,settings_scanlines?UI_OPTIONS_SCANLINES_ON:UI_OPTIONS_SCANLINES_OFF);
            button(94,settings_tv_filter?UI_OPTIONS_TV_ON:UI_OPTIONS_TV_OFF);
            button(126,UI_OPTIONS_START_SCREEN);button(158,UI_AUDIO_TITLE);
        }else{
            button(30,settings_bgm?UI_AUDIO_BGM_ON:UI_AUDIO_BGM_OFF);
            button(62,settings_effects?UI_AUDIO_EFFECTS_ON:UI_AUDIO_EFFECTS_OFF);
            button(94,settings_voice?UI_AUDIO_VOICE_ON:UI_AUDIO_VOICE_OFF);
            button(126,volumes[settings_level]);button(158,UI_AUDIO_GENERAL_TITLE);
        }
        rect(bottom,10,35+32*settings_selected,2,14,C(8,24,27));
    }else if(mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI){
        text(bottom,8,5,144,mode==SLOT_SAVE_UI?UI_SLOT_SAVE:UI_SLOT_LOAD,C(8,24,27));text(bottom,164,5,84,UI_OFFICE_BACK,C(8,24,27));
        for(unsigned i=0;i<SAVE_SLOT_COUNT;i++){
            int y=28+31*i;panel(bottom,8,y,240,29);outline(bottom,8,y,240,29);
            text(bottom,20,y+9,66,slot_labels[i],C(28,30,31));
            if(slot_status[i]==SAVE_OK){
                text(bottom,94,y+9,54,slot_summaries[i].act==1?UI_SLOT_ACT1:slot_summaries[i].act==2?UI_SLOT_ACT2:slot_summaries[i].act==3?UI_SLOT_ACT3:UI_SLOT_ACT_UNKNOWN,C(28,30,31));
                if(slot_summaries[i].place<SAVE_PLACE_COUNT)
                    text(bottom,154,y+9,86,slot_places[slot_summaries[i].place],C(28,30,31));
            }else text(bottom,100,y+9,140,slot_status[i]==SAVE_MISSING?UI_SLOT_EMPTY:UI_SLOT_INVALID,C(28,30,31));
            if(i==slot_selected)rect(bottom,10,y+7,2,15,C(8,24,27));
        }
    }else if(mode==NOTICE){
        if(notice_text==UI_SYS_SAVED){panel(bottom,32,64,192,48);outline(bottom,32,64,192,48);text(bottom,95,82,120,notice_text,C(31,31,31));}
        else{panel(bottom,8,30,240,114);text(bottom,16,42,216,notice_text,C(31,31,31));}
        button(166,UI_SYS_BACK);
    }else if(mode==CONFIRM){
        panel(bottom,8,30,240,66);text(bottom,16,42,216,confirm_action==1?UI_SYS_LOAD_CONFIRM:UI_SYS_TITLE_CONFIRM,C(31,31,31));
        button(106,UI_SYS_YES);button(142,UI_SYS_NO);
        rect(bottom,10,111+36*system_selected,2,14,C(8,24,27));
    }else if(mode==STORY_INPUT_UI){
        button(166,UI_OFFICE_NEXT);
    }else if(mode==STORY_KEYBOARD_UI){
        draw_keyboard();
    }else if(mode==STORY_DIALOG_UI){
        if(!story_error)text(bottom,16,8,176,speaker_labels[story_speaker],C(8,24,27));
        panel(bottom,192,1,56,22);outline(bottom,192,1,56,22);text(bottom,208,6,24,UI_OPTIONS_MENU,C(31,31,31));
        if(header_focus)rect(bottom,194,5,2,14,C(8,24,27));
        panel(bottom,8,24,240,132);
        text(bottom,16,28,216,story_error?UI_OFFICE_ERROR:story_page_read(story_page_start[story_text]+story_page),C(31,31,31));
        if(story_error){
            extern volatile unsigned shared_played_fault,stream_recovery_fault;
            char diagnostic[32];
            snprintf(diagnostic,sizeof(diagnostic),"V101 A%02X S%X R%X",music_error,shared_played_fault,stream_recovery_fault);
            keyboard_text(16,64,diagnostic);
            snprintf(diagnostic,sizeof(diagnostic),"PC%04X OP%02X",story_event_pc,story_event_op);
            keyboard_text(16,82,diagnostic);
        }
        button(166,UI_OFFICE_NEXT);
    }else if(mode==STORY_MENU_UI && flashlight_aim_ui && flashlight_available()){
        draw_flashlight();
    }else if(mode==STORY_MENU_UI){
        text(bottom,8,5,96,story_module()==0x40?UI_STORY_FACTORY:(story_scene&0xff00)==HQ_SCENE(0x1d00)?slot_places[SAVE_PLACE_QUEENS]:(story_scene&0xff00)==HQ_SCENE(0x0100)?slot_places[SAVE_PLACE_HOSPITAL]:((story_scene&0xff00)==HQ_SCENE(0x0500) || (story_scene&0xff00)==HQ_SCENE(0x0600))?slot_places[SAVE_PLACE_FREDDY]:(story_scene&0xff00)==HQ_SCENE(0x1700)?slot_places[SAVE_PLACE_IVAN]:(story_scene&0xff00)==HQ_SCENE(0x1c00)?slot_places[SAVE_PLACE_APARTMENT]:(story_scene&0xff00)==HQ_SCENE(0x1b00)?slot_places[SAVE_PLACE_OUTER]:(story_scene&0xff00)==HQ_SCENE(0x0700)?slot_places[SAVE_PLACE_GIBSON]:(story_scene&0xff00)==HQ_SCENE(0x0000)?slot_places[SAVE_PLACE_ALTON]:(story_scene&0xff00)==HQ_SCENE(0x1800)?slot_places[SAVE_PLACE_PLATO]:story_scene==HQ_SCENE(0x2003)?UI_STORY_VEHICLE:story_scene>4 || story_scene==2?UI_STORY_HEADQUARTERS:story_scene>=3?UI_STORY_OFFICE:UI_RECEPTION_TITLE,C(8,24,27));
        if(flashlight_available()){panel(bottom,8,1,88,22);outline(bottom,8,1,88,22);text(bottom,16,6,72,UI_FLASHLIGHT_LIGHT,C(31,31,31));if(header_focus==3)rect(bottom,10,5,2,14,C(8,24,27));}
        panel(bottom,104,1,48,22);outline(bottom,104,1,48,22);text(bottom,116,6,24,UI_OPTIONS_MENU,C(31,31,31));
        if(header_focus==1)rect(bottom,106,5,2,14,C(8,24,27));
        if(header_focus==2)rect(bottom,160,5,2,14,C(8,24,27));
        text(bottom,164,5,84,UI_OFFICE_BACK,C(8,24,27));story_layout();clip_top=24;clip_bottom=180;
        for(unsigned i=0;i<story_menu_count;i++){
            const ChoiceCell *cell=&story_choices.cells[i];int y=24+cell->y-story_scroll;
            panel(bottom,cell->x,y,cell->width,cell->height);outline(bottom,cell->x,y,cell->width,cell->height);
            text(bottom,cell->x+8,y+7,cell->text_width,story_page_read(story_page_start[story_menu_text[i]]),story_menu_enabled[i]?C(31,31,31):C(13,17,18));
            if(!header_focus && i==(unsigned)story_selected)rect(bottom,cell->x+2,y+5,2,cell->height-10,C(8,24,27));
        }
        clip_top=0;clip_bottom=192;
        if(story_choices.height>156){int w=240*156/story_choices.height,x=8+(240-w)*story_scroll/(story_choices.height-156);rect(bottom,8,184,240,4,C(2,5,7));rect(bottom,x,184,w,4,C(8,24,27));}
    }else if(mode==STORY_VOICE_UI || mode==STORY_GAP_UI || mode==STORY_VOICE_START_UI || (mode==STORY_WAIT_UI && story_audio_running())){
        unsigned voice=story_audio_voice;
        int contact=contact_voice_index(voice);
        if((voice>=77 && voice<=169) || (voice>=181 && voice<=201) || contact>=0){
            draw_portrait_group();
            if(!draw_portraits_only)panel(bottom,8,82,240,84);
            if(!draw_portraits_only && story_audio_caption_active && (mode==STORY_VOICE_UI || mode==STORY_WAIT_UI)){
                const uint16_t *caption=contact>=0?story_page_read(STORY_CONTACT_CAPTION_BASE+contact_caption_starts[contact]+voice_page_index):voice>=181?story_page_read(STORY_CAPTION_BASE_HEADQUARTERS_VOICE_CAPTIONS+(voice-181)):voice<=85?story_page_read(STORY_CAPTION_BASE_STORY_VOICE_CAPTIONS+(voice-77)):voice<=94?story_page_read(STORY_CAPTION_BASE_VOICE_CAPTION_PAGES+(voice_caption_start[voice-86]+voice_page_index)):voice<=102?story_page_read(STORY_CAPTION_BASE_GRANT_CAPTIONS+(voice-95)):voice<=120?story_page_read(STORY_CAPTION_BASE_EQUIPMENT_CAPTIONS+(voice-103)):voice<=136?story_page_read(STORY_CAPTION_BASE_DISPATCH_CAPTIONS+(voice-121)):voice<=143?story_page_read(STORY_CAPTION_BASE_CASE_CAPTIONS+(voice-137)):story_page_read(STORY_CAPTION_BASE_COMBAT_STORY_CAPTIONS+(combat_story_caption_starts[voice-144]+voice_page_index));
                if(contact<0 || !contact_caption_ends[contact] || story_audio_samples()<contact_caption_ends[contact])
                    text(bottom,16,88,216,caption,C(31,31,31));
            }
        }
    }else if(mode==STORY_FAILURE_UI){
        button(82,UI_COMBAT_LOAD);button(118,UI_OPTIONS_START_SCREEN);rect(bottom,10,87+36*system_selected,2,14,C(8,24,27));
    }else if(mode==STORY_WAIT_UI){
        /* CD speech and native timed waits use the same original live
         * portrait projection as PCM speech. Keep all64 rows above text. */
        draw_portrait_group();
        if((music_player_story_cd_track()>=15 && music_player_story_cd_track()<=18)){
            unsigned played;
            if(music_player_story_cd_sample(&played))for(unsigned i=0;i<ACT3_CD_CAPTIONS;i++){
                if(music_player_story_cd_track()==act3_cd_track[i] && played>=act3_cd_start[i] && played<act3_cd_end[i]){
                    panel(bottom,8,82,240,84);text(bottom,16,88,216,story_page_read(STORY_CAPTION_BASE_ACT3_CD_CAPTIONS+(i)),C(31,31,31));break;
                }
            }
        }

        if(music_player_story_cd_track()>=12 && music_player_story_cd_track()<=14){
            unsigned played;
            if(music_player_story_cd_sample(&played))for(unsigned i=0;i<SP33_CD_CAPTIONS;i++){
                if(music_player_story_cd_track()==sp33_cd_track[i] && played>=sp33_cd_start[i] && played<sp33_cd_end[i]){
                    panel(bottom,8,82,240,84);text(bottom,16,88,216,story_page_read(sp33_cd_page_ids[i]),C(31,31,31));break;
                }
            }
        }
        if(music_player_story_cd_track()==21){
            unsigned played;
            if(music_player_story_cd_sample(&played))for(unsigned i=0;i<RECONSTRUCT_CD_CAPTIONS;i++){
                if(played>=reconstruct_cd_start[i] && played<reconstruct_cd_end[i]){
                    panel(bottom,8,82,240,84);text(bottom,16,88,216,story_page_read(reconstruct_cd_page_ids[i]),C(31,31,31));break;
                }
            }
        }
        if(music_player_story_cd_track()>=10 && music_player_story_cd_track()<=11){
            unsigned played;
            if(music_player_story_cd_sample(&played))for(unsigned i=0;i<CRASH_CD_CAPTIONS;i++){
                if(music_player_story_cd_track()==crash_cd_track[i] && played>=crash_cd_start[i] && played<crash_cd_end[i]){
                    panel(bottom,8,82,240,84);text(bottom,16,88,216,story_page_read(STORY_CAPTION_BASE_CRASH_CD_CAPTIONS+(i)),C(31,31,31));break;
                }
            }
        }
        if(music_player_story_cd_track()>=8 && music_player_story_cd_track()<=9){
            unsigned played;
            if(music_player_story_cd_sample(&played))for(unsigned i=0;i<ACT2_CD_CAPTIONS;i++){
                if(music_player_story_cd_track()==act2_cd_track[i] && played>=act2_cd_start[i] && played<act2_cd_end[i]){
                    panel(bottom,8,82,240,84);text(bottom,16,88,216,story_page_read(STORY_CAPTION_BASE_ACT2_CD_CAPTIONS+(i)),C(31,31,31));break;
                }
            }
        }
        if(music_player_story_cd_track()==7){
            unsigned played;
            if(music_player_story_cd_sample(&played))for(unsigned i=0;i<STORY_CD7_CAPTIONS;i++){
                if(played>=story_cd7_start[i] && played<story_cd7_end[i]){
                    panel(bottom,8,82,240,84);text(bottom,16,88,216,story_page_read(STORY_CAPTION_BASE_STORY_CD7_CAPTIONS+(i)),C(31,31,31));break;
                }
            }
        }
    }else if(mode==RECEPTION_DIALOG){
        text(bottom,16,8,216,speaker_labels[4],C(8,24,27));panel(bottom,8,24,240,132);
        text(bottom,16,28,216,reception_texts[office_line],C(31,31,31));button(166,UI_OFFICE_NEXT);
    }else if(mode==RECEPTION_ROOT){
        text(bottom,8,5,216,UI_RECEPTION_TITLE,C(8,24,27));
        for(int i=0;i<4;i++){
            int y=30+30*i;panel(bottom,8,y,240,26);outline(bottom,8,y,240,26);
            text(bottom,20,y+7,108,reception_root_labels[i],i==0?C(28,30,31):C(13,17,18));
            if(i)text(bottom,164,y+7,72,UI_ROOT_PENDING,C(13,17,18));
        }
        text(bottom,8,166,216,UI_SYS_HINT,C(8,24,27));
    } else if(mode==OFFICE_ROOT) {
        panel(bottom,104,1,48,22);outline(bottom,104,1,48,22);text(bottom,116,6,24,UI_OPTIONS_MENU,C(31,31,31));
        text(bottom,8,5,216,UI_ROOT_TITLE,C(8,24,27));
        for(int i=0;i<4;i++) {
            int y=30+30*i;
            panel(bottom,8,y,240,26);outline(bottom,8,y,240,26);
            text(bottom,20,y+7,108,office_root_labels[i],i==1?C(28,30,31):C(13,17,18));
            if(i!=1)text(bottom,164,y+7,72,UI_ROOT_PENDING,C(13,17,18));
        }
        button(166,UI_ROOT_REPLAY);
        text(bottom,8,148,216,UI_SYS_HINT,C(8,24,27));
    } else if(mode==OFFICE_MENU) {
        text(bottom,8,5,96,UI_OFFICE_LOOK,C(8,24,27));
        panel(bottom,104,1,48,22);outline(bottom,104,1,48,22);text(bottom,116,6,24,UI_OPTIONS_MENU,C(31,31,31));
        text(bottom,164,5,84,UI_OFFICE_BACK,C(8,24,27));
        for(unsigned i=0;i<OFFICE_MENU_COUNT;i++)
            button(30+28*i,office_pages[office_page_start[office_menu_text[i]]]);
    } else if(mode==OFFICE_DIALOG) {
        if(!office_error)text(bottom,16,8,176,speaker_labels[office_speakers[office_line]],C(8,24,27));
        panel(bottom,192,1,56,22);outline(bottom,192,1,56,22);text(bottom,208,6,24,UI_OPTIONS_MENU,C(31,31,31));
        panel(bottom,8,24,240,132);
        if(office_error)text(bottom,16,28,216,UI_OFFICE_ERROR,C(31,31,31));
        else text(bottom,16,28,216,office_pages[office_page_start[office_queue[office_line]]+office_page],C(31,31,31));
        button(166,office_error || (office_line+1==office_queue_count && office_page+1==office_page_count[office_queue[office_line]])?UI_OFFICE_BACK:UI_OFFICE_NEXT);
    } else if(mode==DIALOGUE) {
        text(bottom,16,8,216,speaker_labels[LEGACY_SPEAKER],C(8,24,27));
        panel(bottom,8,28,240,84);
        text(bottom,16,36,224,text_0,C(31,31,31));
        text(bottom,16,168,224,text_2,C(8,24,27));
    } else if(mode==MENU) {
        // Dialogue and choices occupy separate states.
        text(bottom,8,5,108,UI_VOICE_SCENE,C(8,24,27));
        text(bottom,128,5,120,text_3,C(28,30,31));
        layout();clip_top=24;clip_bottom=180;
        for(int i=0;i<6;i++) {
            int y=24+row_top[i]-scroll;
            panel(bottom,8,y,240,row_height[i]);
            outline(bottom,8,y,240,row_height[i]);
            text(bottom,20,y+6,216,label(i),C(31,31,31));
        }
        clip_top=0;clip_bottom=192;
        if(menu_total>156) {
            int w=240*156/menu_total,x=8+(240-w)*scroll/(menu_total-156);
            rect(bottom,8,184,240,4,C(2,5,7));rect(bottom,x,184,w,4,C(8,24,27));
        }
    } else if(mode==SELECTED) {
        // This is a label confirmation, not an invented original story response.
        text(bottom,16,12,224,text_9,C(8,24,27));
        text(bottom,16,36,216,label(selected),C(31,31,31));
        text(bottom,16,132,224,text_12,C(15,18,20));
        text(bottom,16,168,224,text_13,C(8,24,27));
    } else {
        memcpy(bottom,battle_pixels,sizeof(bottom));
        theme(top);
        // A small casing indicator loops on the upper screen during combat.
        rect(top,5,24+(frame/8)%12,2,2,C(12,7,2));
        text(top,86,16,120,text_6,C(10,27,29));
        text(bottom,16,8,228,text_7,C(31,31,31));
        panel(bottom,8,30,240,116);
        for(int i=0;i<3;i++)rect(bottom,8+i*80,30,1,116,C(3,10,11));
        rect(bottom,tx-18,ty-18,36,36,flash?C(31,26,12):C(8,24,27));
        rect(bottom,tx-12,ty-12,24,24,C(2,5,6));
        rect(bottom,tx-1,ty-16,2,32,C(31,31,31));rect(bottom,tx-16,ty-1,32,2,C(31,31,31));
        text(bottom,16,148,96,text_8,C(31,31,31));
        glyph(bottom,76,148,digit_glyphs[(hits/10)%10],C(31,31,31));glyph(bottom,88,148,digit_glyphs[hits%10],C(31,31,31));
        button(166,text_4);
    }
}
static void fit_upper_picture(void){
    unsigned w=256,h=192;
    if(chapter_preroll || gameover_visible()){w=256;h=192;}
    else if(mode==BOOT_UI || mode==OPENING || mode==CASE_FLIGHT_UI || mode==CREDITS_UI)opening_picture_size(&w,&h);
    else if(mode==TITLE || mode==ACT_SELECT_UI || mode==SPLASH_UI || ((mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI) && slot_back==TITLE))h=224;
    else if(((mode>=STORY_DIALOG_UI && mode<=STORY_VOICE_START_UI) || mode==STORY_KEYBOARD_UI || mode==STORY_INPUT_UI) ||
        ((mode==SYSTEM || mode==SETTINGS_UI || mode==NOTICE || mode==CONFIRM || mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI) &&
        (resume_mode==STORY_DIALOG_UI || resume_mode==STORY_MENU_UI)) ||
        mode==RECEPTION_DIALOG || mode==RECEPTION_ROOT ||
        (mode<TITLE && mode!=COMBAT && mode!=DIALOGUE)) {w=192;h=128;}
    /* The user's reference shows the original 256x224 raster on a4:3
     * display. Preserve that displayed picture ratio (7:6 pixel aspect),
     * not square raw tiles. Native-size English cards/license retain their
     * legible 1:1 presentation; UI artwork is already authored for the DS. */
    if(w!=256 || h!=192){w*=7;h*=6;}
    bool native_room=w==192*7 && h==128*6 && (story_room_visible() || mode==RECEPTION_DIALOG || mode==RECEPTION_ROOT);
    bool practice_room=native_room && reception_background_matches(0x0e00);
    bool terminal=native_room && reception_background_terminal();
    if(practice_room)w=224*7;
    if(terminal){w=256*7;h=144*6;}
    unsigned cinematic_x,cinematic_w,cinematic_rows;
    if(native_room && reception_background_fullframe()){
        upper_layout_apply_raster(3,256,192,0,256,192);
    }else if(native_room && reception_background_cinematic(&cinematic_x,&cinematic_w,&cinematic_rows)){
        upper_layout_apply_raster(3,cinematic_w*7,cinematic_rows*6,cinematic_x,cinematic_w,cinematic_rows);
    }else upper_layout_apply_raster(3,w,h,terminal?0:practice_room?16:native_room?32:0,terminal?256:practice_room?224:native_room?192:256,terminal?144:native_room?128:192);
    /* Publish the neighbour view with the new picture transform, including
     * a mode change whose completed bitmap was queued at VBlank. */
    video_filters_vblank(upper_game_picture(),
        mode==STORY_COMBAT_UI);

}
static void fade_counter_picture(const uint16_t *source,uint16_t *dest){
    /* Smoothstep over32 presented frames. Preserve opaque source black and
     * leave the separate decorative layers and lower screen untouched. */
    unsigned step=counter_entry_fade-1;
    unsigned level=(step*step*(96-2*step)+64)>>7;
    uint16_t red[32],green[32],blue[32];
    for(unsigned i=0;i<32;i++){
        unsigned value=(i*level+128)>>8;
        red[i]=value;green[i]=value<<5;blue[i]=value<<10;
    }
    for(unsigned i=0;i<256*192;i++){
        unsigned pixel=source[i];
        dest[i]=(pixel&0x8000)|red[pixel&31]|green[(pixel>>5)&31]|blue[(pixel>>10)&31];
    }
}
int main(void) {
    videoSetMode(MODE_5_2D);videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);vramSetBankC(VRAM_C_SUB_BG);
    int a=bgInit(3,BgType_Bmp16,BgSize_B16_256x256,0,0);
    int b=bgInitSub(3,BgType_Bmp16,BgSize_B16_256x256,0,0);
    lcdMainOnTop();top_vram=(uint16_t*)bgGetGfxPtr(a);bottom_vram=(uint16_t*)bgGetGfxPtr(b);
    /* Keep the boot viewport opaque while native assets/audio are loading. */
    for(unsigned i=0;i<256*192;i++)top[i]=0x8000;
    dmaFillWords(0,top_vram,256*256*2);
    dmaFillHalfWords(0x8000,top_vram,sizeof(top));
    system_perf_start();video_filters_init();irqSet(IRQ_VBLANK,game_vblank);
    soundEnable();save_init();save_set_io_service(music_player_service_io);
    save_set_native_buffer_io(capture_native_room,reception_background_checkpoint_restore);
    settings_load();chapter_progress_load();story_set_chapter_recorder(chapter_record);office_reset();mode=BOOT_UI;
    native_entry_set_random_io(story_random_state,story_set_random_state);
    story_set_random_provider(story_native_random);
    if(!upper_backdrop_begin() || !opening_begin(2))notice(UI_SYS_OPENING_ERROR,SPLASH_UI);
    draw();DC_FlushRange(top,sizeof(top));DC_FlushRange(bottom,sizeof(bottom));
    system_perf_wait_vblank();picture_dma_copy(top,top_vram,sizeof(top));picture_dma_copy(bottom,bottom_vram,sizeof(bottom));
    int previous_visual[32]={-1};unsigned previous_reception=~0u;
    uint32_t previous_combat[15]={0};
    unsigned previous_slide_offset=0;
    int previous_opening_stage=-1;
    bool previous_skip_act1=false;
    while(1) {
    unsigned iteration_display=display_frames;
    audio_trace_sample(mode,story_scene,frame);
    if(story_error)runtime_failure_record(2,mode,story_scene);
#ifdef SNATCHER_ENTRY_PROFILE
        static bool factory_clock_started;
        bool profile_factory=mode==STORY_MENU_UI && reception_background_active() && (reception_source_scene>>8)==0x19;
        if(profile_factory && !factory_clock_started){cpuStartTiming(0);factory_clock_started=true;}
        unsigned factory_start=profile_factory?cpuGetTiming():0;
        unsigned factory_display=display_frames,factory_generation=reception_generation;
        unsigned factory_io=music_bus_io_cycles,factory_pcm=factory_pcm_render_cycles;
        unsigned factory_tick=factory_room_tick_cycles,factory_draw=factory_room_draw_cycles;
#endif
#ifdef SNATCHER_OPENING_PROFILE
        bool profile_opening=mode==OPENING && opening_stage<=1 && !opening_native_display();
        uint32_t profile_start=profile_opening?cpuGetTiming():0;
#endif
        if(!opening_native_display()){
            if(mode!=CREDITS_UI && !music_player_slice_active())music_player_update();
        }
        else if(mode!=CASE_FLIGHT_UI && mode!=OPENING && !opening_scene_update()){
            opening_end();story_dispatch(STORY_ERROR);
        }
#ifdef SNATCHER_ENTRY_PROFILE
        unsigned factory_audio_done=profile_factory?cpuGetTiming():0;
#endif
        /* The queued source buffers remain immutable throughout synthesis.
         * Finish their VBlank upload before input/logic may draw again. */
        while(combat_present_pending)system_perf_wait_vblank();
        InputEdge input=poll_input();int down=input.down,held=input.held,up=input.up;
        /* An explicit diagnostic chord only. Opening Menu must never wait
         * for trace SD writes or their repeated audio reserve servicing. */
        if(mode==SYSTEM && (input.down&KEY_X) &&
           (input.held&(KEY_L|KEY_R))==(KEY_L|KEY_R)){
            show_audio_diagnostics();down&=~KEY_X;held&=~KEY_X;
        }

        if(!settings_buttons){down&=KEY_TOUCH;held&=KEY_TOUCH;up&=KEY_TOUCH;}
        touchPosition p=input.touch;
        story_bind_flashlight(reception_background_flashlight_target);
        if(mode==STORY_MENU_UI && flashlight_aim_ui && flashlight_available()){
            int x=0,y=0;reception_background_flashlight_position(&x,&y);
            if((down&(KEY_A|KEY_B)) || ((down&KEY_TOUCH) && p.py<24 && p.px>=160)){flashlight_aim_ui=false;header_focus=3;}
            else if((down&KEY_START) || ((down&KEY_TOUCH) && p.py<24 && p.px>=104 && p.px<152))open_system();
            else if((held&KEY_TOUCH) && p.px>=16 && p.px<240 && p.py>=60 && p.py<180){
                x=-71+(p.px-16)*139/223;y=-39+(p.py-60)*75/119;
                if(!reception_background_flashlight_touch(x,y,true))story_dispatch(STORY_ERROR);
            }else if(held&(KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT)){
                /* DS pad adaptation: whole native coordinates per poll. The
                 * source's CPU busy-loop step is not a display-frame rate. */
                x+=!!(held&KEY_RIGHT)-!!(held&KEY_LEFT);y+=!!(held&KEY_DOWN)-!!(held&KEY_UP);
                if(x<-71)x=-71;
                if(x>68)x=68;
                if(y<-39)y=-39;
                if(y>36)y=36;
                if(!reception_background_flashlight_touch(x,y,true))story_dispatch(STORY_ERROR);
            }
            if(!(held&KEY_TOUCH) || !flashlight_aim_ui){unsigned target;if(!reception_background_flashlight_target(&target))story_dispatch(STORY_ERROR);}
            down=held=up=0;dragged=1;
        }
#ifdef SNATCHER_OPENING_PROFILE
        uint32_t profile_poll=profile_opening?cpuGetTiming():0;
#endif
        if(start_screen_fade){down=held=up=0;update_start_screen_fade();}
        if(mode==STORY_COMBAT_UI && story_practice_active){
            bool fire=combat_queued_fire,toggle=(down&KEY_X)!=0;combat_queued_fire=false;
            if(held&(KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT)){
                int row=(held&KEY_UP)?0:(held&KEY_DOWN)?2:1,col=(held&KEY_LEFT)?0:(held&KEY_RIGHT)?2:1;combat_aim=row*3+col;
            }else if(!(held&KEY_TOUCH) && !fire && !reception_background_practice_value(0x78fa))combat_aim=4;
            if(down&KEY_A)fire=true;
            if(down&KEY_TOUCH){int cell=reception_background_practice_touch(p.px,p.py);if(cell>=0){combat_aim=cell;fire=true;}}
            if(fire && !reception_background_practice_value(0x78f8)){toggle=true;fire=false;combat_queued_fire=true;}
            unsigned row=combat_aim/3,col=combat_aim%3;
            reception_background_practice_input((row==0?1:row==2?2:0)|(col==0?4:col==2?8:0),
                (toggle?64:0)|(fire?32:0));
        }else if(mode==STORY_COMBAT_UI){
            bool fire=combat_queued_fire,toggle=(down&KEY_X)!=0;combat_queued_fire=false;
            if(held&(KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT)){
                int row=(held&KEY_UP)?0:(held&KEY_DOWN)?2:1,col=(held&KEY_LEFT)?0:(held&KEY_RIGHT)?2:1;combat_aim=(unsigned)(row*3+col);
            }else if(!(held&KEY_TOUCH))combat_aim=4;
            if(down&KEY_A)fire=true;
            if(down&KEY_TOUCH){int cell=combat_touch_cell(p.px,p.py);if(cell>=0){combat_aim=(unsigned)cell;fire=true;}}
            if(fire && !combat_state.weapon){toggle=true;fire=false;combat_queued_fire=true;}
            unsigned shots=combat_state.shots;
            unsigned result=combat_tick(combat_aim,fire,toggle);
            if(combat_state.shots!=shots){combat_flash_origin=display_frames+1;combat_has_flash=true;}
            combat_feedback_diagnostic[1]=battle_feedback_face(combat_state.damage);
            combat_feedback_diagnostic[2]=combat_state.damage;
            if(result==COMBAT_LOST)combat_death_ticks++;
            combat_feedback_diagnostic[3]=combat_death_ticks;
            if(!music_player_combat_effects(combat_effects())){story_dispatch(STORY_ERROR);continue;}
            if(result==COMBAT_WON || (result==COMBAT_LOST && combat_death_ticks>=45))finish_battle();
        }
        if(mode==BATTLE_SLIDE_IN || mode==BATTLE_SLIDE_OUT){
            if(++battle_tick>BATTLE_SLIDE_FRAMES){
                if(mode==BATTLE_SLIDE_IN)mode=story_battle_active?STORY_COMBAT_UI:COMBAT;
                else if(story_battle_active){
                    unsigned damage=combat_state.damage;combat_assets_end();story_battle_active=false;
                    bool good;
                    if(story_practice_active){
                        uint16_t result[7];good=reception_background_practice_finish(result) &&
                            ((story_native_battle_stage==0 || story_native_battle_stage==11)?story_practice_result_with_damage(result,damage):story_native_battle_result(result,damage));
                        story_practice_active=false;
                    }
                    else good=((story_scene!=HQ_SCENE(0x1903) && story_scene!=HQ_SCENE(0x1900)) || damage>=6 || reception_background_restore_source(0x19,story_scene&255)) && story_battle_result(damage);
                    if(!good)story_dispatch(STORY_ERROR);else story_dispatch(story_advance());
                }else mode=battle_return_mode;
            }
        }
        else if(mode==BOOT_UI){int result=opening_update(top);if(result<0)notice(UI_SYS_OPENING_ERROR,SPLASH_UI);else if(result>0)mode=SPLASH_UI;}
        else if(mode==CREDITS_UI && !start_screen_fade){
            native_credits_input((down&(KEY_A|KEY_START)) || ((down&KEY_TOUCH) && p.py>=166));
            int result=opening_update(top);
            opening_caption=native_credits_waiting()?-3:-1;
            unsigned played=native_credits_position(opening_sample),track=native_credits_track();
            for(unsigned i=0;i<ACT3_CD_CAPTIONS;i++)if(track==act3_cd_track[i] && played>=act3_cd_start[i] && played<act3_cd_end[i]){
                opening_caption=(int)i;break;
            }
            unsigned quote=native_credits_quote_lines();
            if(quote)opening_caption=-10-(int)quote;
            else if(native_credits_waiting())opening_caption=-3;
            if(result<0){opening_end();story_dispatch(STORY_ERROR);}
            else if(result>0){chapter_clear_pending=!chapter_progress_clear();begin_screen_transition(1,CREDITS_UI);}
        }
        else if(mode==OPENING){int result=opening_update(top);if(result<0)notice(UI_SYS_OPENING_ERROR,TITLE);else if(result>0)finish_opening(false);}
        else if(mode==CASE_FLIGHT_UI){
            int result=opening_update(top);unsigned samples;
            (void)samples; /* Original flight faces follow the played source snapshot below. */
            if(result){
                portrait_motion_end();flight_resource=flight_scene=false;
                if(result<0){opening_end();story_dispatch(STORY_ERROR);}
                else if(opening_stage==5){
                    if(!opening_return_finish(top))story_dispatch(STORY_ERROR);
                    else{story_scene=HQ_SCENE(0x1003);story_dispatch(story_after_native_return());}
                }else story_dispatch(opening_stage==4?story_after_native_forecourt():story_after_native_flight());
            }
        }
        else update_story();
#ifdef SNATCHER_OPENING_PROFILE
        uint32_t profile_update=profile_opening?cpuGetTiming():0;
#endif
        if(!voice_locked()) {
            if(down&KEY_START){
                if(mode==SPLASH_UI){title_selected=0;mode=TITLE;}
                else if(mode==TITLE)title_action(title_selected);
                else if(mode==ACT_SELECT_UI)chapter_action(title_selected);
                else if(mode==STORY_INPUT_UI)story_dispatch(story_advance());
                else if(mode==STORY_KEYBOARD_UI)keyboard_press(name_keyboard_confirm(story_keyboard_kind()));
                else if(saveable())open_system();
                else if(mode==SYSTEM)resume_game();
                else if(mode==SETTINGS_UI)settings_back();
                else if(mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI)mode=slot_back;
                else if(mode==OPENING){if(opening_stage && opening_to_game){opening_end();resume_mode=OPENING;mode=SYSTEM;}else finish_opening(true);}
                else if(mode==COMBAT)finish_battle();
                else if(mode<TITLE)begin_battle();
            }
            if(down&KEY_B){
                if(mode==COMBAT)finish_battle();
                else if(mode==TITLE)mode=SPLASH_UI;
                else if(mode==ACT_SELECT_UI){title_selected=2;mode=TITLE;}
                else if(mode==OPENING)finish_opening(true);
                else if(mode==SYSTEM)resume_game();
                else if(mode==SETTINGS_UI)settings_back();
                else if(mode==NOTICE)mode=notice_return;
                else if(mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI)mode=slot_back;
                else if(mode==CONFIRM)mode=confirm_back;
                else if(mode==STORY_KEYBOARD_UI)keyboard_press(name_keyboard_erase(story_keyboard_kind()));
                else if(mode==STORY_DIALOG_UI)story_next();
                else if(mode==STORY_MENU_UI)story_dispatch(story_cancel());
                else if(mode==OFFICE_DIALOG && story_legacy_section)finish_legacy_office(1);
                else if(mode==RECEPTION_DIALOG)mode=RECEPTION_ROOT;
                else if(mode<TITLE)mode=(mode==OFFICE_DIALOG?OFFICE_MENU:mode==OFFICE_MENU || mode==OFFICE_ROOT?OFFICE_ROOT:DIALOGUE);
            }
            if(mode==SPLASH_UI && (down&KEY_A)){title_selected=0;mode=TITLE;}
            else if(mode==TITLE){if(down&KEY_DOWN)title_selected=(title_selected+1)%title_choices();if(down&KEY_UP)title_selected=(title_selected+title_choices()-1)%title_choices();if(down&KEY_A)title_action(title_selected);}
            else if(mode==ACT_SELECT_UI){if(down&KEY_DOWN)title_selected=(title_selected+1)%3;if(down&KEY_UP)title_selected=(title_selected+2)%3;if(down&KEY_A)chapter_action(title_selected);}
            else if(mode==STORY_INPUT_UI && (down&KEY_A))story_dispatch(story_advance());
            else if(mode==OPENING && (down&KEY_A))finish_opening(true);
            else if(mode==RECEPTION_DIALOG && (down&KEY_A))mode=RECEPTION_ROOT;
            else if(mode==STORY_KEYBOARD_UI){
                if(down&KEY_X)story_dispatch(story_cancel());
                else{
                    if(down&KEY_LEFT)name_keyboard_move_for(&name_keyboard,story_keyboard_kind(),-1,0);
                    if(down&KEY_RIGHT)name_keyboard_move_for(&name_keyboard,story_keyboard_kind(),1,0);
                    if(down&KEY_UP)name_keyboard_move_for(&name_keyboard,story_keyboard_kind(),0,-1);
                    if(down&KEY_DOWN)name_keyboard_move_for(&name_keyboard,story_keyboard_kind(),0,1);
                    if(down&(KEY_LEFT|KEY_RIGHT|KEY_UP|KEY_DOWN))story_generation++;
                    if(down&KEY_A)keyboard_press(name_keyboard.selected);
                }
            }
            else if(mode==STORY_DIALOG_UI){
                if(down&KEY_UP)header_focus=1;
                if(down&KEY_DOWN)header_focus=0;
                if(down&KEY_A){if(header_focus)open_system();else story_next();}
            }
            else if(mode==STORY_FAILURE_UI){
                if(down&(KEY_UP|KEY_DOWN))system_selected=!system_selected;
                if(down&KEY_A){if(!system_selected)open_slots(false,STORY_FAILURE_UI);else{confirm_action=2;confirm_back=STORY_FAILURE_UI;system_selected=1;mode=CONFIRM;}}
            }
            else if(mode==STORY_MENU_UI){
                story_layout();
                if(header_focus){
                    if(header_focus==3 && !flashlight_available())header_focus=1;
                    if(down&KEY_RIGHT)header_focus=header_focus==3?1:2;
                    if(down&KEY_LEFT)header_focus=header_focus==2?1:flashlight_available()?3:1;
                    if(down&KEY_DOWN)header_focus=0;
                    if(down&KEY_A){if(header_focus==1)open_system();else if(header_focus==2)story_dispatch(story_cancel());else if(header_focus==3)flashlight_aim_ui=true;}
                }else{
                    if(down&KEY_RIGHT)story_selected=choice_step(&story_choices,story_selected,1,0);
                    if(down&KEY_LEFT)story_selected=choice_step(&story_choices,story_selected,-1,0);
                    if(down&KEY_DOWN)story_selected=choice_step(&story_choices,story_selected,0,1);
                    if(down&KEY_UP){if(story_choices.cells[story_selected].y==0)header_focus=flashlight_available() && story_choices.cells[story_selected].x<104?3:1;else story_selected=choice_step(&story_choices,story_selected,0,-1);}
                    if(down&(KEY_RIGHT|KEY_LEFT|KEY_DOWN|KEY_UP))story_focus();
                    if((down&KEY_A) && !header_focus && story_menu_enabled[story_selected])story_dispatch(story_pick(story_selected));
                }
            }
            else if(mode==NOTICE && (down&KEY_A))mode=notice_return;
            else if(mode==SYSTEM){if(down&KEY_DOWN)system_selected=(system_selected+1)%3;if(down&KEY_UP)system_selected=(system_selected+2)%3;if(down&KEY_A)system_action(system_selected);}
            else if(mode==SETTINGS_UI){if(down&KEY_DOWN)settings_selected=(settings_selected+1)%5;if(down&KEY_UP)settings_selected=(settings_selected+4)%5;if(down&KEY_A)settings_action(settings_selected);}
            else if(mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI){if(down&KEY_DOWN)slot_selected=(slot_selected+1)%SAVE_SLOT_COUNT;if(down&KEY_UP)slot_selected=(slot_selected+SAVE_SLOT_COUNT-1)%SAVE_SLOT_COUNT;if(down&KEY_A)slot_action(slot_selected);}
            else if(mode==CONFIRM){if(down&(KEY_DOWN|KEY_UP))system_selected=!system_selected;if(down&KEY_A)confirm_choice(!system_selected);}
        }
        if(down&KEY_TOUCH) {
            if(voice_locked()) { /* Sequence owns progression. */ }
            else if(mode==BOOT_UI){ /* Logos advance automatically. */ }
            else if(mode==SPLASH_UI){title_selected=0;mode=TITLE;}
            else if(mode==TITLE){if(p.px>=8 && p.px<248 && p.py>=58 && p.py<58+36*title_choices() && (p.py-58)%36<30)title_action((p.py-58)/36);}
            else if(mode==ACT_SELECT_UI){if(p.py<30 && p.px>=164){title_selected=2;mode=TITLE;}else if(p.px>=8 && p.px<248 && p.py>=58 && p.py<160 && (p.py-58)%36<30)chapter_action((p.py-58)/36);}
            else if(mode==OFFICE_DIALOG && !office_error && p.py<24 && p.px>=192)open_system();
            else if((mode==OFFICE_MENU || mode==OFFICE_ROOT) && p.py<24 && p.px>=104 && p.px<152)open_system();
            else if(mode==OPENING){if(p.py>=166)finish_opening(true);}
            else if(mode==STORY_INPUT_UI){if(p.py>=166)story_dispatch(story_advance());}
            else if(mode==STORY_KEYBOARD_UI){
                if(p.py<24 && p.px>=164)story_dispatch(story_cancel());
                else{int key=name_keyboard_hit_for(story_keyboard_kind(),p.px,p.py);if(key>=0)keyboard_press(key);}
            }
            else if(mode==STORY_DIALOG_UI){if(p.py<24 && p.px>=192)open_system();else if(p.py>=24)story_next();}
            else if(mode==STORY_FAILURE_UI){
                if(p.py>=82 && p.py<106)open_slots(false,STORY_FAILURE_UI);
                else if(p.py>=118 && p.py<142){confirm_action=2;confirm_back=STORY_FAILURE_UI;system_selected=1;mode=CONFIRM;}
            }
            else if(mode==STORY_MENU_UI){
                if(p.py<24 && p.px>=8 && p.px<96 && flashlight_available()){flashlight_aim_ui=true;dragged=1;}
                else if(p.py<24 && p.px>=104 && p.px<152)open_system();
                else if(p.py<24 && p.px>=164)story_dispatch(story_cancel());
                else if(p.py>=24 && p.py<180){drag_x=p.px;drag_start=p.py;drag_last=p.py;dragged=0;}
            }
            else if(mode==RECEPTION_DIALOG){if(p.py>=166)mode=RECEPTION_ROOT;}
            else if(mode==RECEPTION_ROOT){if(p.px>=8 && p.px<248 && p.py>=30 && p.py<56){office_line=1;mode=RECEPTION_DIALOG;}}
            else if(mode==SYSTEM){if(p.py<24 && p.px>=164)resume_game();else if(p.px>=8 && p.px<248 && p.py>=30 && p.py<118 && (p.py-30)%32<24)system_action((p.py-30)/32);}
            else if(mode==SETTINGS_UI){if(p.py<24 && p.px>=164)settings_back();else if(p.px>=8 && p.px<248 && p.py>=30 && p.py<182 && (p.py-30)%32<24)settings_action((p.py-30)/32);}
            else if(mode==SLOT_SAVE_UI || mode==SLOT_LOAD_UI){if(p.py<24 && p.px>=164)mode=slot_back;else if(p.px>=8 && p.px<248 && p.py>=28 && p.py<181 && (p.py-28)%31<29)slot_action((p.py-28)/31);}
            else if(mode==NOTICE){if(p.py>=166)mode=notice_return;}
            else if(mode==CONFIRM){
                if(p.py>=106 && p.py<130)confirm_choice(true);
                else if(p.py>=142 && p.py<166)confirm_choice(false);
            }
            else if(mode==OFFICE_ROOT) {
                if(p.py>=166)replay_voice_scene();
                else if(p.px>=8 && p.px<248 && p.py>=60 && p.py<86)mode=OFFICE_MENU;
            }
            else if(mode==OFFICE_MENU) {
                if(p.py<24){if(p.px>=128)mode=OFFICE_ROOT;else replay_voice_scene();}
                else if(p.px>=8 && p.px<248 && p.py>=30 && p.py<170 && (p.py-30)%28<24) {
                    office_error=!office_choose((p.py-30)/28);office_line=office_page=0;
                    if(office_error || office_queue_count)mode=OFFICE_DIALOG;
                }
            }
            else if(mode==OFFICE_DIALOG) {
                if(office_error)mode=OFFICE_MENU;
                else if(++office_page<office_page_count[office_queue[office_line]]) { }
                else {office_page=0;if(++office_line>=office_queue_count){if(story_legacy_section)finish_legacy_office(1);else mode=OFFICE_MENU;}}
            }
            else if(mode==DIALOGUE){mode=MENU;scroll=0;dragged=1;}
            else if(mode==SELECTED)mode=DIALOGUE;
            else if(mode==MENU) {
                if(p.py<24 && p.px>=128)begin_battle();
                else if(p.py<24)replay_voice_scene();
                else {drag_x=p.px;drag_start=p.py;drag_last=p.py;dragged=0;}
            } else if(mode==COMBAT && p.py>=166)finish_battle();
            else if(mode==COMBAT && p.px>=tx-18 && p.px<tx+18 && p.py>=ty-18 && p.py<ty+18) {
                hits++;flash=8;tx=32+(hits*67)%192;ty=54+(hits*29)%66;
            }
        }
        if(mode==MENU && (held&KEY_TOUCH) && !dragged) {
            if(p.py-drag_start>6 || drag_start-p.py>6)dragged=2;
        }
        if(mode==MENU && (held&KEY_TOUCH) && dragged==2) {
            scroll+=drag_last-p.py;layout();drag_last=p.py;
        }
        if(mode==MENU && (up&KEY_TOUCH) && !dragged && drag_x>=8 && drag_x<248 && drag_start>=24 && drag_start<180) {
            layout();int y=drag_start-24+scroll;
            for(int i=0;i<6;i++)if(y>=row_top[i] && y<row_top[i]+row_height[i]){selected=i;mode=SELECTED;break;}
        }
        if(mode==MENU && (held&KEY_DOWN)){scroll+=3;layout();}
        if(mode==MENU && (held&KEY_UP)){scroll-=3;layout();}
        if(mode==STORY_MENU_UI && (held&KEY_TOUCH)){
            if(!dragged && (p.py-drag_start>6 || drag_start-p.py>6))dragged=2;
            if(dragged==2){story_scroll+=drag_last-p.py;story_layout();drag_last=p.py;}
        }
        if(mode==STORY_MENU_UI && (up&KEY_TOUCH) && !dragged && drag_x>=8 && drag_x<248 && drag_start>=24 && drag_start<180){
            story_layout();int i=choice_hit(&story_choices,drag_x,drag_start-24+story_scroll);
            if(i>=0 && story_menu_enabled[i])story_dispatch(story_pick((unsigned)i));
        }
        // Static artwork/text needs no repaint while polling input and voice timers.
        // Avoid copying both full screens on every tick just to show the same image.
        if(!reception_background_update(story_audio_running()?2:0)){
            story_error=1;reception_background_end();
            if(story_practice_active){combat_assets_end();story_practice_active=story_battle_active=false;story_dispatch(STORY_ERROR);}
        }
        if(story_practice_active && mode==STORY_COMBAT_UI){
            combat_state.damage=reception_background_practice_value(0x7910);
            combat_feedback_diagnostic[1]=battle_feedback_face(combat_state.damage);
            combat_feedback_diagnostic[2]=combat_state.damage;
            unsigned shots=reception_background_practice_value(0x7908);
            if(shots!=practice_shots){practice_shots=shots;combat_flash_origin=display_frames+1;combat_has_flash=true;}
            if(reception_background_practice_done()){
                theme(battle_pixels);
                if(!reception_background_practice_draw(battle_pixels,(uint8_t*)hq_pixels))story_error=1;
                finish_battle();
            }
        }
        unsigned perf_prepare=system_perf_enter(PERF_DRAW);
        native_entry_display_prepare();
        system_perf_leave(perf_prepare);
        /* Input/source handoffs have finished and the next native picture
         * is prepared. Refill before the potentially expensive UI draw.
         * Never advance the shared sound clock before source dispatch. */
        if(music_player_slice_active())music_player_update_slice();
        if(portrait_live_active && voice_locked()){
            bool good;
            if(mode==CASE_FLIGHT_UI || opening_native_display()){
                const resource_portrait_snapshot *p=opening_portraits();
                good=!p || portrait_live_snapshot(p);
            }else good=portrait_live_source(reception_background_video(),reception_source_epoch);
            if(!good){story_error=1;mode=STORY_DIALOG_UI;}
        }
        int flashlight_key=0;
        if(mode==STORY_MENU_UI && flashlight_aim_ui && flashlight_available()){
            int x=0,y=0;reception_background_flashlight_position(&x,&y);flashlight_key=65536+(x+71)+((y+39)<<8);
        }
        int visual[32]={mode,0,selected,long_menu,scroll,(int)hits,tx,(mode==STORY_MENU_UI?flashlight_key:ty),flash,(int)office_line,(int)office_page,office_error,voice_page_index,opening_frame,opening_caption,title_selected,(int)portrait_generation,(int)story_generation,(int)story_scene,(int)story_page,story_error,story_selected,story_scroll,(int)system_selected,(int)(settings_selected+8*settings_general),settings_buttons,(int)(settings_level|(settings_bgm<<8)|(settings_effects<<9)|(settings_voice<<10)),(int)header_focus,(int)slot_selected,settings_scanlines,settings_tv_filter,story_audio_caption_active};
#ifdef SNATCHER_ENTRY_PROFILE
        unsigned factory_update_done=profile_factory?cpuGetTiming():0;
#endif
        unsigned perf_draw=system_perf_enter(PERF_DRAW);
        bool room_dirty=story_room_visible() && previous_reception!=reception_generation;
        /* Moving the lamp changes one lower cursor, not the whole menu.
         * Preserve all source ticks/upper publications; restore only the
         * old/new cursor rows from the same preblended panel artwork. */
        bool flashlight_partial=flashlight_key && flashlight_key!=previous_visual[7] && previous_visual[7]>=65536 && !counter_entry_fade;
        for(unsigned i=0;i<32 && flashlight_partial;i++)if(i!=7 && visual[i]!=previous_visual[i])flashlight_partial=false;
        uint32_t combat_picture[15]={0};bool combat_dirty=false;
        if(mode==STORY_COMBAT_UI){
            if(story_practice_active){combat_picture[0]=reception_generation;combat_picture[11]=combat_state.damage;}
            else combat_picture_key(combat_picture);
            combat_picture[14]=(frame/8)%12;
            combat_dirty=memcmp(combat_picture,previous_combat,sizeof(combat_picture))!=0;
        }
        int dirty=room_dirty || counter_entry_fade || previous_skip_act1!=opening_act1_title() || memcmp(visual,previous_visual,sizeof(visual)) || (mode==COMBAT && frame%8==0) || combat_dirty || mode==BATTLE_SLIDE_IN || mode==BATTLE_SLIDE_OUT;
        bool upper_only=(mode==CREDITS_UI && previous_visual[0]==CREDITS_UI && previous_visual[14]==opening_caption) ||
            (mode==BOOT_UI && previous_visual[0]==BOOT_UI) ||
            (mode==OPENING && previous_visual[0]==OPENING && previous_visual[14]==opening_caption && previous_opening_stage==(int)opening_stage && previous_skip_act1==opening_act1_title()) ||
            (mode==CASE_FLIGHT_UI && previous_visual[0]==CASE_FLIGHT_UI && previous_visual[14]==opening_caption && previous_visual[16]==(int)portrait_generation);
        bool combat_partial=mode==STORY_COMBAT_UI && previous_visual[0]==STORY_COMBAT_UI &&
            previous_visual[29]==settings_scanlines && previous_visual[30]==settings_tv_filter;
        combat_repaint_top=!combat_partial || combat_picture[14]!=previous_combat[14] || combat_picture[11]!=previous_combat[11];
        combat_repaint_bottom=!combat_partial || memcmp(combat_picture,previous_combat,14*sizeof(uint32_t));
        combat_draw_partial=combat_partial;
        combat_indicator_only=combat_partial && combat_picture[11]==previous_combat[11];
        if(combat_partial){
            if(story_practice_active)for(unsigned y=0;y<192;y++){
                combat_rows[y].left=0;combat_rows[y].right=256;
            }else combat_damage_rows(previous_combat,combat_picture,combat_rows);
        }
        bool sliding=mode==BATTLE_SLIDE_IN || mode==BATTLE_SLIDE_OUT;
        unsigned slide_offset=sliding?battle_slide_offset(battle_tick,mode==BATTLE_SLIDE_OUT):0;
        bool slide_partial=sliding && previous_visual[0]==mode;
        unsigned slide_first=slide_offset<previous_slide_offset?slide_offset:previous_slide_offset;
        unsigned slide_last=(slide_offset>previous_slide_offset?slide_offset:previous_slide_offset)+192;
        const uint16_t *display=top;
        unsigned upper_bytes=combat_partial && !combat_repaint_top?0:sizeof(top);
        unsigned lower_first=0,lower_bytes=sizeof(bottom);
        unsigned combat_top_first=24,combat_top_last=37,combat_top_left=4,combat_top_width=4;
        if(combat_partial){
            unsigned a=24+previous_combat[14],b=24+combat_picture[14];
            combat_top_first=a<b?a:b;combat_top_last=(a>b?a:b)+2;
            if(combat_picture[11]!=previous_combat[11]){combat_top_first=0;combat_top_last=192;combat_top_left=0;combat_top_width=256;}
        }
        top_room_rows_partial=false;
        if(dirty) {
            if(flashlight_partial){
                bool copied=false;
                if(room_dirty && !draw_room_top(&copied)){story_error=1;reception_background_end();}
                if(!copied)upper_bytes=0;
                int old_y=60+(((previous_visual[7]-65536)>>8)&255)*119/75;
                int new_y=60+(((flashlight_key-65536)>>8)&255)*119/75;
                int first=(old_y<new_y?old_y:new_y)-6,last=(old_y>new_y?old_y:new_y)+7;
                if(first<60)first=60;if(last>180)last=180;
                panel(bottom,16,first,224,last-first);outline(bottom,16,60,224,120);draw_flashlight_cursor();
                lower_first=(unsigned)first;lower_bytes=(unsigned)(last-first)*256*2;
            }
            else if(gameover_visible() && !memcmp(visual,previous_visual,sizeof(visual))){
                upper_only=true;if(!draw_gameover_top())story_error=1;
            }
            else if(room_dirty && !gameover_visible() && !counter_entry_fade && !memcmp(visual,previous_visual,sizeof(visual))){
                upper_only=true;
                bool copied=false;
                if(!draw_room_top(&copied)){
                    story_error=1;reception_background_end();upper_only=false;
                }else if(!copied)upper_bytes=0;
            }
            slide_draw_partial=slide_partial;
            slide_draw_old=previous_slide_offset;slide_draw_new=slide_offset;
            if(!upper_only && !flashlight_partial){
                draw_portraits_only=return_portrait_only(visual,previous_visual,previous_opening_stage) ||
                    ((mode==STORY_VOICE_UI || mode==STORY_GAP_UI || mode==STORY_VOICE_START_UI ||
                    (mode==STORY_WAIT_UI && story_audio_running())) &&
                    !memcmp(visual,previous_visual,16*sizeof(*visual)) &&
                    !memcmp(visual+17,previous_visual+17,15*sizeof(*visual)));
                draw();
                if(draw_portraits_only){
                    lower_first=8;lower_bytes=64*256*2;
                    if(reception_background_active() && !counter_entry_fade)upper_bytes=256*reception_background_picture_rows()*2;
                }
                draw_portraits_only=false;
                if(!draw_upper_changed && !counter_entry_fade && previous_visual[30]==settings_tv_filter)upper_bytes=0;
            }
            slide_draw_partial=false;
            /* A native room has192x128 dots at x32. When only its next
             * picture changes, the already-cleared lower64 staging rows
             * stay untouched. All room sprites still tick and compose;
             * avoid flushing/transferring that unused32KiB every frame. */
            if(upper_bytes && upper_only && !gameover_visible() && reception_background_active() && !counter_entry_fade)
                upper_bytes=256*reception_background_picture_rows()*2;
            if(top_room_indexed && upper_bytes)upper_bytes=256*192;
            if(slide_partial)slide_transfer(slide_first,slide_last,true);
            else if(upper_bytes && top_room_indexed)top_room_transfer(true);
            else if(upper_bytes && opening_palette_display())DC_FlushRange(top,256*192);
            else if(upper_bytes && !opening_native_display()){
                if(settings_tv_filter && upper_game_picture()){video_filters_copy(top,battle_pixels,256*192);display=battle_pixels;}
                if(counter_entry_fade){fade_counter_picture(display,battle_pixels);display=battle_pixels;}
                DC_FlushRange(display,upper_bytes);
            }
            if(combat_partial){if(combat_repaint_bottom)combat_transfer_rows(true);}
            else if(!upper_only && !slide_partial){
                if(bottom_combat_indexed){lower_combat_transfer(true);lower_bytes=256*128;}
                else DC_FlushRange(bottom+lower_first*256,lower_bytes);
            }
            previous_slide_offset=slide_offset;
            memcpy(previous_visual,visual,sizeof(visual));
            memcpy(previous_combat,combat_picture,sizeof(combat_picture));
            previous_opening_stage=opening_stage;previous_skip_act1=opening_act1_title();
            previous_reception=reception_generation;
        }
#ifdef SNATCHER_OPENING_PROFILE
        uint32_t profile_ready=profile_opening?cpuGetTiming():0;
        unsigned profile_line=REG_VCOUNT;
#endif
#ifdef SNATCHER_ENTRY_PROFILE
        unsigned factory_draw_done=profile_factory?cpuGetTiming():0;
        unsigned factory_line=REG_VCOUNT;
#endif
        /* A crossed VBlank already paced an unchanged picture. */
        system_perf_leave(perf_draw);
        bool upload=dirty && ((upper_bytes && !opening_native_display()) || !upper_only || (top_room_indexed && top_room_palette_pending));
        bool native_lower=opening_native_display();
        bool audio_full=upload && music_player_slice_active() &&
            (native_lower || (!opening_native_display() && !opening_palette_display()));
        bool combat_deferred=dirty && (combat_partial || audio_full);
        bool media_present_now=false;int media_present_lock=0;
        /* Only a finished indexed upper page (49152 bytes plus its palette)
         * may use this VBlank. Reads/composition/cache flushes are complete.
         * Line208 leaves at least54 whole blank lines, over3.4ms. The measured
         * worst upload is2.05ms; IRQ masking prevents an interrupt consuming
         * that margin. Filtered pages keep their existing presentation path. */
        if(upload && upper_only && !combat_deferred && !slide_partial &&
           !settings_tv_filter && !settings_scanlines && opening_palette_display() &&
           (mode==BOOT_UI || (mode==OPENING && opening_stage<=1))){
            media_present_lock=enterCriticalSection();
            unsigned line=REG_VCOUNT;
            media_present_now=line>=192 && line<=208;
            if(!media_present_now)leaveCriticalSection(media_present_lock);
        }
        if(combat_deferred){
            int lock=enterCriticalSection();
            audio_present_slide=slide_partial;
            audio_present_first=slide_first;audio_present_last=slide_last;
            audio_present_full=!combat_partial;
            /* Native upper pixels are presented independently in the same IRQ.
             * Queue only the finished lower page; service audio until upload. */
            audio_present_upper_bytes=native_lower?0:upper_bytes;audio_present_lower=!upper_only;
            audio_present_lower_first=lower_first;audio_present_lower_bytes=upper_only?0:lower_bytes;
            audio_present_pixels=display;
            combat_present_top=upper_bytes!=0;combat_present_bottom=combat_repaint_bottom;
            combat_present_first=combat_top_first;combat_present_last=combat_top_last;combat_present_left=combat_top_left;combat_present_width=combat_top_width;
            combat_present_pending=1;leaveCriticalSection(lock);
         }else if(!media_present_now && (upload || (!music_player_slice_active() && display_frames==iteration_display)))system_perf_wait_vblank();
#ifdef SNATCHER_ENTRY_PROFILE
        unsigned factory_wait_done=profile_factory?cpuGetTiming():0;
#endif
#ifdef SNATCHER_OPENING_PROFILE
        uint32_t profile_wait=profile_opening?cpuGetTiming():0;
#endif
        if(dirty && !combat_deferred && slide_partial){upper_bitmap_mode(false);lower_bitmap_mode(false);fit_upper_picture();slide_transfer(slide_first,slide_last,false);}
        else if(dirty && !combat_deferred){
            if(top_room_indexed && top_room_palette_pending)upper_bitmap_mode(true);
            if(upper_bytes && !opening_native_display()){
                if(!opening_palette_display())upper_bitmap_mode(top_room_indexed);
                fit_upper_picture();
                if(opening_palette_display())opening_present_palette((const uint8_t*)top);
                else{
                /* VBlank's filter setup preceded this new viewport. Publish
                 * its blur transform/window together with the first black
                 * counter frame, avoiding one frame of covered decoration. */
                if(counter_entry_fade==1)video_filters_vblank(true,false);
                if(combat_partial){
                    for(unsigned y=combat_top_first;y<combat_top_last;y++)picture_dma_copy(display+y*256+combat_top_left,top_vram+y*256+combat_top_left,combat_top_width*2);
                }else if(top_room_indexed && display==top)top_room_transfer(false);
                else picture_dma_copy(display,top_vram,upper_bytes);
                }
            }if(combat_partial){if(combat_repaint_bottom)combat_transfer_rows(false);}
            else if(!upper_only){
                if(bottom_combat_indexed)lower_combat_transfer(false);
                else {lower_bitmap_mode(false);picture_dma_copy(bottom+lower_first*256,bottom_vram+lower_first*256,lower_bytes);}
            }}
        if(media_present_now)leaveCriticalSection(media_present_lock);
#ifdef SNATCHER_OPENING_PROFILE
        if(profile_opening && mode==OPENING && opening_palette_display()){
            volatile uint32_t *row=opening_loop_profile[opening_loop_profile_index++&31];
            row[0]=opening_stage;row[1]=opening_frame;row[2]=profile_start;
            row[3]=profile_poll;row[4]=profile_update;row[5]=profile_ready;
            row[6]=profile_wait;row[7]=cpuGetTiming();row[8]=dirty;row[9]=upper_only;
            row[10]=profile_line;row[11]=REG_VCOUNT;
            for(unsigned i=0;i<3;i++)row[12+i]=opening_audio_profile[i];
            row[15]=frame;
        }
#endif
        if(counter_entry_fade && ++counter_entry_fade>33)counter_entry_fade=0;
        if(music_player_slice_active() || combat_present_pending){
            do{
                bool serviced=music_player_update_slice();
                if(!serviced && (display_frames==iteration_display || combat_present_pending))system_perf_wait_vblank();
            }while(display_frames==iteration_display || combat_present_pending);
        }
        /* Publish the centered portrait and complete lower field before
         * the original READY/controller work can occupy several displays. */
        if(mode==STORY_COMBAT_UI && story_practice_active)reception_background_practice_run();
        if(start_screen_fade && screen_transition_phase==1){
            /* All direct/deferred framebuffer work above has completed. */
            int lock=enterCriticalSection();start_screen_origin=display_frames;
            screen_transition_phase=2;leaveCriticalSection(lock);
        }
        frame++;if(flash)flash--;
#ifdef SNATCHER_ENTRY_PROFILE
        if(profile_factory){
            volatile uint32_t *row=factory_loop_profile[factory_loop_profile_index&31];
            row[0]=mode;row[1]=factory_start;row[2]=factory_audio_done;
            row[3]=factory_update_done;row[4]=factory_draw_done;row[5]=cpuGetTiming();
            row[6]=display_frames-factory_display;row[7]=reception_generation-factory_generation;
            row[8]=music_bus_io_cycles-factory_io;row[9]=factory_pcm_render_cycles-factory_pcm;
            row[10]=factory_room_tick_cycles-factory_tick;row[11]=factory_room_draw_cycles-factory_draw;
            row[12]=factory_line;row[13]=frame;row[14]=factory_wait_done;row[15]=music_written;
            factory_loop_profile_index++;
        }
#endif
    }
}
