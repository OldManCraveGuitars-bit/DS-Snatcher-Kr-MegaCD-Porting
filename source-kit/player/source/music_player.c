#include "resource_pcm_archive.h"
#include "resource_cdda_stream.h"
static resource_cdda_stream story_cd;
static unsigned story_cd_track;
#include "system_perf.h"
#include "music_player.h"
#include "resource_music.h"
#include "resource_fm.h"
#include "resource_archive.h"
#include "resource_room_audio.h"
#include "resource_combat_sound.h"
#include "resource_shared_sound.h"
#include "native_entry.h"
#include "return_audio_queue.h"
#include "native_entry_display.h"
static void return_retire(unsigned played){
    /* The live presenter latches the running timer after its VRAM clear.
     * Its valid cursor can trail Maxmod slightly. Retain both consumers. */
    unsigned shown=native_entry_display_played();
    native_entry_return_retire(played<shown?played:shown);
}
#include "settings.h"
#include <nds.h>
#include <maxmod9.h>
#include <string.h>
#include "audio_refill_budget.h"
volatile unsigned audio_refill_calls,audio_refill_samples,audio_refill_low;
#include <stdlib.h>
static bool ready,streaming,entry,flight,cd_playing;
static bool music_servicing;
static return_audio_queue return_queue;
#define RETURN_BLOCK_FRAMES 512u
static int16_t *return_blocks[NATIVE_RETURN_RESERVE_FRAMES/RETURN_BLOCK_FRAMES];
volatile unsigned return_reserve_current,return_reserve_peak,return_reserve_generated,return_reserve_delivered;
static bool return_reserve_pump(void);
static void return_reserve_end(void){
    if(return_queue.blocks){
        for(unsigned i=0;i<sizeof(return_blocks)/sizeof(*return_blocks);i++){free(return_blocks[i]);return_blocks[i]=NULL;}
    }else free(return_queue.samples);
    memset(&return_queue,0,sizeof(return_queue));return_reserve_current=0;
}
static resource_shared_sound *shared_audio;
static uint64_t shared_stream_origin;
static bool shared_reconstructing;
static bool shared_wait_active,shared_wait_music;
static unsigned shared_wait_frame,shared_wait_remaining;
#define MUSIC_STREAM_BUFFER_FRAMES 12288
static bool music_player_recovery_begin(void);
static void music_player_stream_ready(void);
static bool shared_begin(void),shared_command(unsigned,unsigned),shared_played(void);
static bool shared_start_stream(void),shared_request(unsigned,unsigned);
static bool shared_render(int16_t *,unsigned);
volatile unsigned shared_game_requests,shared_game_waits,shared_game_completions,shared_game_timeouts,shared_game_status;
volatile unsigned shared_game_epoch,shared_game_fill_count;
#ifdef SNATCHER_AUDIO_HASH
volatile unsigned shared_game_fills[4096],shared_game_hashes[4096];
#endif
volatile unsigned shared_game_event_count,shared_game_events[1024][4];
static void shared_event(unsigned kind,unsigned a,unsigned b){
    unsigned at=shared_game_event_count&1023;
    shared_game_events[at][0]=shared_audio?shared_audio->samples:0;
    shared_game_events[at][1]=kind;shared_game_events[at][2]=a;shared_game_events[at][3]=b;
    shared_game_event_count++;
}
/* Manual Maxmod servicing is non-reentrant. CD, score and room/effect
 * mixing consume their temporary samples sequentially; no caller retains
 * these bytes across the next stage. Keep both nested1KiB arrays off the
 *6KiB user stack used by source actors and the original FM interpreter. */
static int16_t music_mix_scratch[512];

static bool music_sliced;
static unsigned music_slice_remaining,music_primed;
static FILE *cd_file;
static unsigned cd_frames,cd_position;
volatile unsigned flight_cd_start,flight_cd_played;
#ifdef SNATCHER_ENTRY_PROFILE
volatile unsigned flight_fill_cycles,flight_mix_cycles,flight_cd_cycles,flight_music_cycles;
volatile unsigned factory_pcm_render_cycles;
#endif
static resource_music bank,arrival_bank,forecourt_bank,effect_bank;
static resource_room_audio *room_audio;
static resource_combat_sounds combat_sounds;
volatile unsigned combat_sound_requests[5],combat_sound_error;
static uint8_t *drum_commands;
static uint8_t *room_table,*room_resident;
static size_t room_table_bytes,room_resident_bytes;
volatile unsigned music_written,music_played,music_error,music_command,music_missing;
volatile unsigned music_drum_loops;
volatile unsigned music_loops,music_start_sample;
volatile int music_min_lead=0x7fffffff;
volatile int music_min_fill_lead=0x7fffffff;
bool music_player_init(void){
    if(!ready){ready=mmInitNoSoundbank();if(ready)mmLockChannels((1u<<0)|(1u<<1));}
    return ready;
}
void music_player_end(void){
    resource_cdda_close(&story_cd);story_cd_track=0;
    if(streaming)mmStreamClose();
    return_reserve_end();
    if(shared_audio){
        resource_shared_sound_end(shared_audio);
        if(room_audio==&shared_audio->pcm)room_audio=NULL;
        free(shared_audio);shared_audio=NULL;
    }
    shared_wait_active=false;shared_stream_origin=0;
    streaming=false;resource_fm_close();
    resource_room_audio_drums_end(room_audio);free(drum_commands);drum_commands=NULL;
    if(entry)native_entry_end();
    entry=false;
    resource_music_close(&bank);music_command=0;
    resource_music_close(&arrival_bank);
    resource_music_close(&forecourt_bank);
    resource_music_close(&effect_bank);
    if(cd_file)fclose(cd_file);
    cd_file=NULL;cd_playing=flight=false;cd_frames=cd_position=0;
}
static bool request_music(unsigned command){
    if(shared_audio)return shared_request(command,1);
    if(flight && command==255){music_command=0;music_start_sample=~0u;return true;}
    if(flight && command==0x4c){
        /* Prepared before DAC start; activate on the original source command.
         * No file stream is read and no voice/actor sample is discarded. */
        if(!resource_fm_active())return false;
        resource_music_close(&bank);music_command=command;
        music_start_sample=native_entry_current_sample();return true;
    }
    if(flight && command==0x45){
        resource_music *next=&arrival_bank;
        if(!next->file || next->position)return false;
        resource_music_close(&bank);bank=*next;memset(next,0,sizeof(*next));
        music_command=command;music_start_sample=native_entry_current_sample();return true;
    }
    if(command!=(flight?0x69u:0x4fu)){music_missing=command;return false;}
    if(!bank.file || bank.command!=command || bank.position)return false;
    music_command=command;music_start_sample=native_entry_current_sample();return true;
}
static unsigned read32(const uint8_t *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static bool start_cd(void *context,unsigned track){
    (void)context;
    if(track!=(native_entry_is_return()?6u:20u) || !cd_file || cd_playing || fseek(cd_file,24,SEEK_SET))return false;
    cd_position=flight_cd_played=0;cd_playing=true;flight_cd_start=native_entry_current_sample();return true;
}
static bool pause_cd(void *context){(void)context;cd_playing=false;return true;}
static int16_t status_cd(void *context){(void)context;return cd_playing?0x100:0;}
static unsigned bcd(unsigned n){return n/10*16+n%10;}
static bool mix_music(int16_t *out,unsigned frames){
#ifdef SNATCHER_ENTRY_PROFILE
    unsigned begin=cpuGetTiming();
#endif
    int16_t *pcm=music_mix_scratch;
    while(frames){
        unsigned n=frames>256?256:frames;
        if(!(music_command==0x4c?resource_fm_render(pcm,n):resource_music_read(&bank,pcm,n))){music_error=3;return false;}
        if(settings_bgm)for(unsigned i=0;i<n*2;i++){
            int value=out[i]+pcm[i];out[i]=value>32767?32767:value< -32768?-32768:value;
        }
        out+=n*2;frames-=n;
    }
    if(music_command!=0x4c)music_loops=bank.loops;
#ifdef SNATCHER_ENTRY_PROFILE
    flight_music_cycles+=cpuGetTiming()-begin;
#endif
    return true;
}
static uint32_t clock_cd(void *context){
    (void)context;unsigned f=(uint64_t)cd_position*75/32000;
    return bcd(f/4500)<<24|bcd(f/75%60)<<16|bcd(f%75)<<8;
}
static bool mix_cd(void *context,int16_t *out,unsigned frames){
    (void)context;int16_t *start=out;unsigned total=frames;
#ifdef SNATCHER_ENTRY_PROFILE
    unsigned begin=cpuGetTiming();
#endif
    if(!cd_playing)frames=0;
    else if(frames>cd_frames-cd_position)frames=cd_frames-cd_position;
    int16_t *pcm=music_mix_scratch;
    while(frames){
        unsigned n=frames>256?256:frames;
        if(system_perf_fread(pcm,4,n,cd_file)!=n)return false;
        if(settings_bgm)for(unsigned i=0;i<n*2;i++){
            int value=out[i]+pcm[i];out[i]=value>32767?32767:value< -32768?-32768:value;
        }
        out+=n*2;frames-=n;cd_position+=n;
    }
    flight_cd_played=cd_position;if(cd_position==cd_frames)cd_playing=false;
#ifdef SNATCHER_ENTRY_PROFILE
    flight_cd_cycles+=cpuGetTiming()-begin;
#endif
    /* Music commands can change inside a Maxmod refill. Mix each source
     * segment here so score45 never replaces the preceding score69 samples. */
    return shared_audio || !music_command || mix_music(start,total);
}
static mm_word fill_body(mm_word frames,mm_addr destination,mm_stream_formats format){
#ifdef SNATCHER_ENTRY_PROFILE
    unsigned begin=cpuGetTiming();
#endif
    /* Maxmod copies this work buffer to the DAC ring only after
     * the callback returns. Publish complete FM+PCM chunks during blocking
     * asset reads instead of withholding a whole long catch-up refill.
     * Startup still primes its complete buffer before the DAC is enabled. */
    frames=audio_refill_take(frames,&music_slice_remaining,music_sliced,
        streaming && (shared_audio || (resource_fm_active() && music_command!=0x4c)));
    if(!frames)return 0;
    int16_t *out=destination;
    if(format!=MM_STREAM_16BIT_STEREO){music_error=1;return 0;}
    if(shared_audio){
        if(return_queue.samples){
            unsigned played=mmStreamGetPosition();return_retire(played);
            if(!native_entry_return_reserve_room() && (int)(music_written-played)>=8192)return 0;
        }
        if(entry){
            unsigned buffered=return_audio_queue_read(&return_queue,out,frames);
            return_reserve_delivered+=buffered;return_reserve_current=return_queue.count;
            if(buffered<frames)native_entry_audio(out+buffered*2,frames-buffered);
            if(native_entry_status()<0 || native_entry_status()==2){music_error=2;return 0;}
        }else if(!shared_render(out,frames)){music_error=8;return 0;}
        if(streaming){
            int lead=(int)(music_written-mmStreamGetPosition());
            if(lead<music_min_fill_lead)music_min_fill_lead=lead;
        }
        music_written+=frames;return frames;
    }
    if(entry){native_entry_audio(out,frames);if(native_entry_status()<0 || native_entry_status()==2)music_error=2;}
    else if(resource_fm_active()){
        if(!resource_fm_render(out,frames)){music_error=6;memset(out,0,frames*4);}
    }else memset(out,0,frames*4);
#ifdef SNATCHER_ENTRY_PROFILE
    unsigned mixed=cpuGetTiming();
#endif
    if(!flight && bank.file && music_written+frames>music_start_sample){
        unsigned start=music_start_sample>music_written?music_start_sample-music_written:0;
        mix_music(out+start*2,frames-start);
    }
    if(effect_bank.file){
        int16_t *pcm=music_mix_scratch;unsigned at=0;
        while(at<frames){
            unsigned n=frames-at;if(n>128)n=128;
            if(!resource_music_read(&effect_bank,pcm,n)){music_error=4;break;}
            if(settings_effects)for(unsigned i=0;i<n*2;i++){
                int v=out[at*2+i]+pcm[i];out[at*2+i]=v>32767?32767:v< -32768?-32768:v;
            }
            at+=n;
        }
        if(effect_bank.position==effect_bank.frames)resource_music_close(&effect_bank);
    }
    if(room_audio){
        int16_t *pcm=music_mix_scratch;unsigned at=0;
        while(at<frames){
            unsigned n=frames-at;if(n>128)n=128;
#ifdef SNATCHER_ENTRY_PROFILE
            unsigned pcm_begin=cpuGetTiming();
#endif
            if(!resource_room_audio_render(room_audio,pcm,n)){music_error=5;break;}
#ifdef SNATCHER_ENTRY_PROFILE
            factory_pcm_render_cycles+=cpuGetTiming()-pcm_begin;
#endif
            for(unsigned i=0;i<n*2;i++){
                int v=out[at*2+i]+pcm[i];out[at*2+i]=v>32767?32767:v< -32768?-32768:v;
            }
            at+=n;
        }
        music_drum_loops=room_audio->drum_loops;
    }
#ifdef SNATCHER_ENTRY_PROFILE
    flight_mix_cycles+=cpuGetTiming()-mixed;flight_fill_cycles+=cpuGetTiming()-begin;
#endif
    if(streaming){
        int lead=(int)(music_written-mmStreamGetPosition());
        if(lead<music_min_fill_lead)music_min_fill_lead=lead;
    }
    music_written+=frames;return frames;
}
static mm_word fill(mm_word frames,mm_addr destination,mm_stream_formats format){
    unsigned previous=system_perf_enter(PERF_AUDIO);
    mm_word result=fill_body(frames,destination,format);
    system_perf_leave(previous);return result;
}
#include "music_player_shared.inc"
static bool entry_begin(bool title){
    music_player_end();music_written=music_played=music_error=music_command=music_missing=music_loops=0;
    music_start_sample=~0u;music_min_lead=music_min_fill_lead=0x7fffffff;
    if(!music_player_init() || !shared_begin())return false;
    resource_entry_audio audio={NULL,&shared_audio->pcm.driver,entry_shared_render,
        entry_shared_command,entry_shared_status,entry_shared_event};
    if(!(title?native_entry_begin_title_audio(request_music,&audio):native_entry_begin_audio(request_music,&audio))){music_player_end();return false;}
    entry=true;
    mm_stream stream={32000,entry?(flight?NATIVE_FLIGHT_BUFFER_FRAMES:NATIVE_ENTRY_BUFFER_FRAMES):MUSIC_STREAM_BUFFER_FRAMES,fill,MM_STREAM_16BIT_STEREO,MM_TIMER2,true};
    mmStreamOpen(&stream);if(!music_written){music_error=7;music_player_end();return false;}music_player_stream_ready();
    if(music_error){music_player_end();return false;}return true;
}
bool music_player_entry_begin(void){return entry_begin(false);}
bool music_player_entry_title_begin(void){return entry_begin(true);}
bool music_player_flight_begin(void){
    music_player_end();music_written=music_played=music_error=music_command=music_missing=music_loops=0;
    music_start_sample=flight_cd_start=~0u;flight_cd_played=0;music_min_lead=music_min_fill_lead=0x7fffffff;
    if(!music_player_init())return false;
#ifdef SNATCHER_ENTRY_PROFILE
    cpuStartTiming(0);flight_fill_cycles=flight_mix_cycles=flight_cd_cycles=0;
#endif
    flight=true;cd_file=fopen("nitro:/native-cdda-20-r22.nsta","rb");
    uint8_t h[24];
    if(!cd_file || system_perf_fread(h,1,24,cd_file)!=24 || memcmp(h,"NST1\1\0\2\0",8) ||
       read32(h+8)!=32000 || read32(h+16)!=24 || read32(h+20)!=16 ||
       !(cd_frames=read32(h+12)) || cd_frames>32000u*1800u ||
       fseek(cd_file,0,SEEK_END) || ftell(cd_file)!=(long)(24+cd_frames*4) || !shared_begin()){
        music_player_end();return false;
    }
    resource_entry_cd cd={NULL,start_cd,pause_cd,status_cd,clock_cd,mix_cd};
    resource_entry_audio audio={NULL,&shared_audio->pcm.driver,entry_shared_render,
        entry_shared_command,entry_shared_status,entry_shared_event};
    shared_reconstructing=true;
    bool reconstructed=native_entry_flight_begin_audio(request_music,&cd,&audio);
    shared_reconstructing=false;
    if(!reconstructed){music_player_end();return false;}
    shared_stream_origin=shared_audio->samples;
    if(!resource_shared_sound_played(shared_audio,shared_stream_origin)){music_player_end();return false;}
    entry=true;
    mm_stream stream={32000,entry?(flight?NATIVE_FLIGHT_BUFFER_FRAMES:NATIVE_ENTRY_BUFFER_FRAMES):MUSIC_STREAM_BUFFER_FRAMES,fill,MM_STREAM_16BIT_STEREO,MM_TIMER2,true};
    mmStreamOpen(&stream);if(!music_written){music_error=7;music_player_end();return false;}music_player_stream_ready();
    if(music_error){music_player_end();return false;}return true;
}
bool music_player_entry_release(void){
    if(!entry || (!native_entry_initial_transferred() && (native_entry_status()!=1 || native_entry_handoff()!=0x5a)))return false;
    native_entry_end();entry=false;return true;
}
bool music_player_forecourt_begin(void){
    music_player_end();music_written=music_played=music_error=music_command=music_missing=music_loops=0;
    music_start_sample=~0u;music_min_lead=music_min_fill_lead=0x7fffffff;
    if(!music_player_init() || !shared_begin())return false;
    flight=true;resource_entry_cd cd={NULL,start_cd,pause_cd,status_cd,clock_cd,mix_cd};
    resource_entry_audio audio={NULL,&shared_audio->pcm.driver,entry_shared_render,
        entry_shared_command,entry_shared_status,entry_shared_event};
    shared_reconstructing=true;
    bool reconstructed=native_entry_forecourt_begin_audio(request_music,&cd,&audio);
    shared_reconstructing=false;
    if(!reconstructed){music_player_end();return false;}
    shared_stream_origin=shared_audio->samples;
    if(!resource_shared_sound_played(shared_audio,shared_stream_origin)){music_player_end();return false;}
    entry=true;music_start_sample=0;
    mm_stream stream={32000,entry?(flight?NATIVE_FLIGHT_BUFFER_FRAMES:NATIVE_ENTRY_BUFFER_FRAMES):MUSIC_STREAM_BUFFER_FRAMES,fill,MM_STREAM_16BIT_STEREO,MM_TIMER2,true};
    mmStreamOpen(&stream);if(!music_written){music_error=7;music_player_end();return false;}music_player_stream_ready();
    if(music_error){music_player_end();return false;}return true;
}
static bool music_player_recovery_begin(void){
    extern bool port_stream_recovery_active(void);
    extern bool port_stream_recovery_enable(unsigned);
    if(port_stream_recovery_active())return true;
    if(!port_stream_recovery_enable(music_written))return false;
    music_primed-=128; /* Same protected retirement margin as the return path. */
    return true;
}
static void music_player_stream_ready(void){
    music_primed=music_written;streaming=true;mmStreamVolume(settings_volume());
    if(shared_audio && !music_player_recovery_begin())music_error=8;
}
void music_player_update(void){
    if(!streaming || music_servicing)return;
    if(shared_audio && !music_player_recovery_begin()){music_error=8;return;}
    music_servicing=true;
    mmStreamVolume(settings_volume());shared_played();mmStreamUpdate();music_played=mmStreamGetPosition();shared_played();
    extern unsigned port_stream_recovery_error(void);
    if (port_stream_recovery_error()) music_error=8;

    int lead=(int)(music_written-music_played);if(lead<music_min_lead)music_min_lead=lead;
    music_servicing=false;
}
unsigned music_player_played(void){return streaming?mmStreamGetPosition():0;}
bool music_player_flight_cdda_sample(unsigned *sample){
    if(!sample || !streaming || !entry || !flight || flight_cd_start==~0u)return false;
    unsigned played=music_player_played();
    if(played<flight_cd_start || played-flight_cd_start>=cd_frames)return false;
    *sample=played-flight_cd_start;return true;
}
void music_player_room_pcm_end(void){
    /* Scene graphics never own the shared chip, including during silence. */
    if(shared_audio)return;
    /* The picture pipeline does not own the background drum voices. Keep
     * the shared original chip/waves while source score63 is still active. */
    if((music_command==0x63 || music_command==0x40 || music_command==0x62) && drum_commands && room_audio)return;
    free(room_audio);free(room_table);free(room_resident);
    room_audio=NULL;room_table=room_resident=NULL;room_table_bytes=room_resident_bytes=0;
}
int music_player_room_pcm_load(unsigned id){
    if(!entry && !shared_begin())return -1;
    FILE *f=fopen("nitro:/native-room-pcm-r22.nrs","rb");if(!f)return -1;
    if(!room_table)room_table=resource_archive_load(f,"PCMLT_01.BIN",&room_table_bytes);
    if(!room_table || room_table_bytes<0x4010 || !id || id>=0x4000/8 || id*8+8>room_table_bytes){fclose(f);return -1;}
    if(room_table[id*8+1]!=1){
        fclose(f);
        if(room_audio && !resource_pcm_driver_effect(&room_audio->driver,255,255))return -1;
        if(shared_audio)shared_event(3,255,255);
        return 0;
    }
    if(!room_resident)room_resident=resource_archive_load(f,"PCMDRMDT.BIN",&room_resident_bytes);
    char name[16];snprintf(name,sizeof(name),"PCM%04u.BIN",id);
    bool good=room_resident!=NULL;
    if(good && !room_audio){
        room_audio=malloc(sizeof(*room_audio));
        resource_pcm_effect_bank input={room_table,room_resident,NULL,room_table_bytes,room_resident_bytes,0};
        good=room_audio && resource_room_audio_bind(room_audio,&input);
    }
    if(good)good=resource_pcm_archive_load(&room_audio->driver,f,name,id);
    fclose(f);
    if(good && shared_audio)shared_event(2,id,0);
    return good?1:-1;
}
bool music_player_room_pcm_effect(unsigned command,unsigned pair){
    if(!room_audio || !resource_pcm_driver_effect(&room_audio->driver,command,pair))return false;
    if(shared_audio)shared_event(3,command,pair);
    if(!streaming){
        if(!music_player_init())return false;
        music_written=music_played=music_error=0;
        mm_stream stream={32000,entry?(flight?NATIVE_FLIGHT_BUFFER_FRAMES:NATIVE_ENTRY_BUFFER_FRAMES):MUSIC_STREAM_BUFFER_FRAMES,fill,MM_STREAM_16BIT_STEREO,MM_TIMER2,true};
        mmStreamOpen(&stream);if(!music_written){music_error=7;music_player_end();return false;}music_player_stream_ready();
    }
    return !music_error;
}
unsigned music_player_room_pcm_flags(void){return room_audio?room_audio->driver.flags:0;}
unsigned music_player_room_pcm_id(void){return room_audio?room_audio->driver.load_id:65535;}
unsigned music_player_room_pcm_attenuation(void){return room_audio?room_audio->driver.attenuation:0;}
bool music_player_room_pcm_set_attenuation(unsigned value){
    if(!room_audio && !value)return true;
    if(value>255 || (!room_audio && !shared_begin()))return false;
    room_audio->driver.attenuation=(uint8_t)value;return true;
}
unsigned music_player_room_pcm_bank(void){
    if(!room_audio || room_audio->driver.load_id==65535)return 0;
    unsigned id=room_audio->driver.load_id;
    const resource_pcm_effect_bank *b=&room_audio->driver.bank;
    /* This slot field restores effect-bank samples, not completed voice
     * streams. Active spoken scenes are locked out of the save UI. */
    return id<0x4000/8 && id*8+8<=b->table_bytes && b->table[id*8+1]==1?id:0;
}
bool music_player_effect(unsigned command){
    if(shared_audio || !entry)return shared_command(command,0);
    /* Native entry/flight continuity is deliberately still a failing
     * integration boundary; do not silently skip an original effect. */
    return false;
#if 0
    /* Original9D effect requests and3D foreground-effect stop. Keep the
     * background score and its stream independent of the finite effect. */
    if(command==0x3d){resource_music_close(&effect_bank);return true;}
    if((command!=0x13 && command!=0x16 && command!=0x1a && command!=0x2e) || flight || !music_player_init())return false;
    const char *path=command==0x13?"nitro:/native-effect-13-r22.nsmu":command==0x16?"nitro:/native-effect-16-r22.nsmu":command==0x1a?"nitro:/native-effect-1a-r22.nsmu":"nitro:/native-effect-2e-r22.nsmu";
    if(!resource_music_open(&effect_bank,path,command))return false;
    if(!streaming){
        music_written=music_played=music_error=0;
        music_min_lead=music_min_fill_lead=0x7fffffff;
        mm_stream stream={32000,entry?(flight?NATIVE_FLIGHT_BUFFER_FRAMES:NATIVE_ENTRY_BUFFER_FRAMES):MUSIC_STREAM_BUFFER_FRAMES,fill,MM_STREAM_16BIT_STEREO,MM_TIMER2,true};
        mmStreamOpen(&stream);if(!music_written){music_error=7;music_player_end();return false;}music_player_stream_ready();
    }
    return !music_error;
#endif
}
bool music_player_combat_begin(unsigned stage){
    memset((void*)combat_sound_requests,0,sizeof(combat_sound_requests));combat_sound_error=0;
    if(!room_audio || room_audio->driver.load_id==65535)return false;
    FILE *f=fopen("nitro:/native-resource-banks-r22.nrs","rb");if(!f)return false;
    size_t bytes=0;uint8_t *data=resource_archive_load(f,"SUBCODE.BIN",&bytes);fclose(f);
    bool good=resource_combat_sound_bind(&combat_sounds,data,bytes,stage);free(data);
    if(good){
        const unsigned commands[]={combat_sounds.hit,combat_sounds.shot,combat_sounds.enemy,combat_sounds.draw};
        for(unsigned i=0;i<4;i++){
            resource_pcm_effect effect;
            if(!resource_pcm_effect_lookup(&room_audio->driver.bank,commands[i],&effect))good=false;
        }
    }
    return good;
}
bool music_player_combat_effects(unsigned events){
    const unsigned bits[]={8,2,1,4};
    const unsigned commands[]={combat_sounds.draw,combat_sounds.shot,combat_sounds.hit,combat_sounds.enemy};
    for(unsigned i=0;i<4;i++)if(events&bits[i]){
        if(!music_player_room_pcm_effect(commands[i],255)){combat_sound_error=commands[i];return false;}
        combat_sound_requests[i]++;
    }
    if(events&16){
        if(!music_player_effect(combat_sounds.damage)){combat_sound_error=0x100|combat_sounds.damage;return false;}
        combat_sound_requests[4]++;
    }
    return true;
}
bool music_player_command(unsigned command){
    if(!command)return true; /* Source zero music request is a no-op. */
    if(shared_audio || !entry)return shared_command(command,1);
    if(command==music_command)return true;
    if(entry && flight){
        /* A StoryVM music request must not free the live scene/history while
         * its VBlank presenter and actors are still running. SP07/00D2
         * explicitly stops the score before warning voice130. */
        resource_fm_close();        resource_music_close(&bank);music_command=0;music_start_sample=~0u;
        if(command==255){music_missing=0;return true;}
        if(command==0x61 && resource_music_open(&bank,"nitro:/native-music-61-r22.nsmu",command)){
            music_command=command;music_missing=0;music_start_sample=native_entry_current_sample();return true;
        }
        music_missing=command;return false;
    }
    if(command==255){
        resource_fm_close();resource_music_close(&bank);resource_room_audio_drums_end(room_audio);
        free(drum_commands);drum_commands=NULL;music_command=music_missing=0;return true;
    }
    if(command==0x5f || command==0x5e){
        music_player_end();music_written=music_played=music_error=music_missing=music_loops=music_start_sample=0;
        music_min_lead=music_min_fill_lead=0x7fffffff;
        if(!music_player_init() || !resource_fm_open(command)){music_player_end();return false;}
        music_command=command;
        mm_stream stream={32000,entry?(flight?NATIVE_FLIGHT_BUFFER_FRAMES:NATIVE_ENTRY_BUFFER_FRAMES):MUSIC_STREAM_BUFFER_FRAMES,fill,MM_STREAM_16BIT_STEREO,MM_TIMER2,true};
        mmStreamOpen(&stream);if(!music_written){music_error=7;music_player_end();return false;}music_player_stream_ready();
        if(music_error){music_player_end();return false;}return true;
    }
    if(command==0x63 || command==0x40 || command==0x62){
        music_player_end();music_written=music_played=music_error=music_missing=music_loops=music_start_sample=0;
        music_min_lead=music_min_fill_lead=0x7fffffff;
        if(!music_player_init() || !(command!=0x63?resource_fm_open(command):resource_music_open(&bank,"nitro:/native-music-63-r22.nsmu",command))){music_player_end();return false;}
        if(!room_audio){
            FILE *f=fopen("nitro:/native-room-pcm-r22.nrs","rb");if(!f)return false;
            if(!room_table)room_table=resource_archive_load(f,"PCMLT_01.BIN",&room_table_bytes);
            if(!room_resident)room_resident=resource_archive_load(f,"PCMDRMDT.BIN",&room_resident_bytes);
            fclose(f);if(!room_table || !room_resident)return false;
            room_audio=malloc(sizeof(*room_audio));resource_pcm_effect_bank input={room_table,room_resident,NULL,room_table_bytes,room_resident_bytes,0};
            if(!room_audio || !resource_room_audio_bind(room_audio,&input))return false;
        }
        FILE *f=fopen(command==0x40?"nitro:/native-music-40-drums-r22.nsdm":command==0x62?"nitro:/native-music-62-drums-r22.nsdm":"nitro:/native-music-63-drums-r22.nsdm","rb");if(!f)return false;
        bool good=!fseek(f,0,SEEK_END);long size=good?ftell(f):0;good=good && size>=32 && size<=1024*1024 && !fseek(f,0,SEEK_SET);
        if(good)drum_commands=malloc(size);
        good=good && drum_commands && system_perf_fread(drum_commands,1,size,f)==(size_t)size &&
            resource_room_audio_drums_begin(room_audio,drum_commands,size);fclose(f);
        if(!good){music_player_end();return false;}
        music_command=command;
        mm_stream stream={32000,entry?(flight?NATIVE_FLIGHT_BUFFER_FRAMES:NATIVE_ENTRY_BUFFER_FRAMES):MUSIC_STREAM_BUFFER_FRAMES,fill,MM_STREAM_16BIT_STEREO,MM_TIMER2,true};
        mmStreamOpen(&stream);if(!music_written){music_error=7;music_player_end();return false;}music_player_stream_ready();
        if(music_error){music_player_end();return false;}return true;
    }
    if(command==0x4f){
        music_player_end();music_written=music_played=music_error=music_missing=music_loops=music_start_sample=0;
        music_min_lead=music_min_fill_lead=0x7fffffff;
        if(!music_player_init() || !resource_music_open(&bank,"nitro:/native-music-4f-r22.nsmu",command))return false;
        music_command=command;
        mm_stream stream={32000,entry?(flight?NATIVE_FLIGHT_BUFFER_FRAMES:NATIVE_ENTRY_BUFFER_FRAMES):MUSIC_STREAM_BUFFER_FRAMES,fill,MM_STREAM_16BIT_STEREO,MM_TIMER2,true};
        mmStreamOpen(&stream);if(!music_written){music_error=7;music_player_end();return false;}music_player_stream_ready();
        if(music_error){music_player_end();return false;}return true;
    }
    /* Do not carry the counter score into a different original music cue.
     * Unsupported tracks remain observable until their converters are ported. */
    music_player_end();music_missing=command;return false;
}

/* Bound storage servicing, while retaining half the primed DAC ring for
 * slow SD reads. Never restart the stream or skip samples/source pictures. */
/* Recovery transport commits at most256 frames per call. A budget greater
 * than256 therefore needs another explicit update, with input/scene work
 * still interleaved by the caller after at most512 generated frames. */
static void music_player_refill_budget(unsigned budget){
    music_slice_remaining=budget;music_sliced=true;
    do{
        unsigned before=music_written;
        music_player_update();
        if(music_error || music_written==before)break;
    }while(music_slice_remaining);
    music_sliced=false;
}
void music_player_service_io(void){
    if(music_servicing)return;
    if(!music_player_slice_active()){music_player_update();return;}
    /* File work must make progress even during a dense source-score bar.
     * The scanline-sized main-loop slices prioritize an imminent picture;
     * using them to chase a moving reserve here can occupy several seconds.
     * Publish bounded512-sample chunks, then return to the pending SD work.
     * Neither the synthesis cursor nor the queued audio is skipped/reset. */
    /* One source quantum per storage checkpoint. Repeatedly synthesizing
     * toward a moving reserve can monopolize each 4KiB I/O boundary when
     * the source score is near its CPU budget. File work must also advance;
     * the next checkpoint/main-loop turn continues at the exact cursor. */
    for(unsigned slice=0;slice<1;slice++){
        unsigned played=mmStreamGetPosition();
        int pending=(int)(played-(music_written-music_primed));
        if(pending<512)break;
        music_player_refill_budget(512);
        unsigned previous=system_perf_enter(PERF_DRAW);
        native_entry_display_prepare();
        system_perf_leave(previous);
        if(music_error || (int)(music_written-music_player_played())>=(int)(music_primed/2))break;
    }
}
bool music_player_update_slice(void){
    if(music_servicing || !music_player_slice_active())return false;
    unsigned played=mmStreamGetPosition();
    int pending=(int)(played-(music_written-music_primed));
    /* VBlank publishes completed pictures independently. Avoid tiny
     * scanline fragments that repeatedly reload PCM state and postpone
     * sound when scene work has already crossed a display boundary. */
    const unsigned quantum=audio_refill_quantum(pending,music_primed);
    if(return_queue.samples){
        return_retire(played);
        if(!native_entry_return_reserve_room() && (int)(music_written-played)>=8192)return false;
    }
    if(pending<(int)quantum)return return_reserve_pump();
    unsigned before=music_written;
    music_player_refill_budget(quantum);
    audio_refill_calls++;audio_refill_samples+=music_written-before;
    if(quantum>256)audio_refill_low++;
    return true;
}
bool music_player_slice_active(void){
    /* Once the original script yields its forecourt dialogue, its sound
     * callback still advances every actor and saves every source picture.
     * Let the UI run between bounded chunks of that same continuous clock.
     * Startup/automatic flight keeps its existing complete priming path. */
    bool forecourt=entry && flight && native_entry_status()==1 &&
        (native_entry_handoff()==0x18 || native_entry_handoff()==0x97);
    return streaming && !music_error && (forecourt || shared_audio || (!entry && resource_fm_active()));
}

bool music_player_return_begin(resource_room *room,uint8_t fields[128]){
    if(entry || !streaming || !shared_audio || cd_file || music_error)return false;
    FILE *return_cd=fopen("nitro:/native-cdda-06-r22.nsta","rb");uint8_t h[24];
    if(!return_cd)return false;
    if(system_perf_fread(h,1,24,return_cd)!=24 || memcmp(h,"NST1\1\0\2\0",8) ||
       read32(h+8)!=32000 || read32(h+16)!=24 || read32(h+20)!=16 || !read32(h+12)){
        fclose(return_cd);return false;
    }
    cd_file=return_cd;cd_frames=read32(h+12);
    resource_entry_audio a={NULL,&shared_audio->pcm.driver,entry_shared_render,entry_shared_command,entry_shared_status,entry_shared_event};
    resource_entry_cd cd={NULL,start_cd,pause_cd,status_cd,clock_cd,mix_cd};
    music_player_update();
    if(!native_entry_return_begin(room,fields,&music_written,&a,&cd)){
        native_entry_end();fclose(cd_file);cd_file=NULL;cd_frames=0;cd_playing=false;
        return false;
    }
    flight=entry=true;flight_cd_start=~0u;flight_cd_played=0;
    return true;
}
bool music_player_return_reserve(void){
    /* The old room has been released and the native presenter is ready.
     * Allocate here, not while both old/new room resources are still live. */
    if(!entry || !native_entry_is_return() || return_queue.samples || music_error)return false;
    unsigned count=sizeof(return_blocks)/sizeof(*return_blocks),made=0;
    for(;made<count;made++){
        return_blocks[made]=malloc(RETURN_BLOCK_FRAMES*4);
        if(!return_blocks[made])break;
    }
    if(made!=count || !return_audio_queue_bind_blocks(&return_queue,return_blocks,count,RETURN_BLOCK_FRAMES)){
        for(unsigned i=0;i<made;i++){free(return_blocks[i]);return_blocks[i]=NULL;}
        return false;
    }
    return_reserve_current=return_reserve_peak=return_reserve_generated=return_reserve_delivered=0;
    if(!music_player_recovery_begin()){return_reserve_end();return false;}

    return true;
}
static bool return_reserve_pump(void){
    if(music_servicing || !streaming || !entry || music_error || !return_queue.samples ||
       !native_entry_is_return() || native_entry_status()<0 || native_entry_status()==2)return false;
    /* The native source has reached its original interactive boundary.
     * Drain all reserved bytes before handing ownership back to StoryVM. */
    if(native_entry_status()==1 && native_entry_handoff()==0x1b1)return false;
    unsigned played=mmStreamGetPosition();return_retire(played);
    if((int)(music_written-played)<8192 || !native_entry_return_reserve_room())return false;
    int16_t *target=NULL;unsigned n=return_audio_queue_write_view(&return_queue,&target,256);
    if(!n)return false;
    music_servicing=true;native_entry_audio(target,n);music_servicing=false;
    if(native_entry_status()<0 || native_entry_status()==2 || music_error){music_error=2;return false;}
    if(!return_audio_queue_publish(&return_queue,n)){music_error=2;return false;}
    return_reserve_generated+=n;return_reserve_current=return_queue.count;
    if(return_queue.count>return_reserve_peak)return_reserve_peak=return_queue.count;
    return true;
}
bool music_player_return_release(void){
    if(!entry || !native_entry_return_transferred())return false;
    if(return_queue.count || return_reserve_generated!=return_reserve_delivered)return false;
    return_reserve_end();native_entry_end();entry=flight=false;
    if(cd_file){fclose(cd_file);}
    cd_file=NULL;cd_playing=false;return true;
}

void music_player_halt_failed(void){if(streaming)mmStreamClose();streaming=false;}

bool music_player_story_cd_begin(unsigned track){
    if((track!=7 && track!=8 && track!=9 && track!=10 && track!=11 && track!=12 && track!=13 && track!=14 && track!=15 && track!=16 && track!=17 && track!=18 && track!=21) || entry || flight || cd_file || music_error ||
       !shared_begin() || !shared_start_stream())return false;
    if(story_cd.frames && resource_cdda_status(&story_cd,music_player_played())!=0)return false;
    resource_cdda_close(&story_cd);
    char path[48];snprintf(path,sizeof(path),"nitro:/native-cdda-%02u-r22.nsta",track);
    if(!resource_cdda_open(&story_cd,path,music_written))return false;
    story_cd_track=track;return true;
}
int music_player_story_cd_status(void){return resource_cdda_status(&story_cd,music_player_played());}
unsigned music_player_story_cd_track(void){return story_cd_track;}
bool music_player_story_cd_sample(unsigned *sample){
    return resource_cdda_position(&story_cd,music_player_played(),sample);
}
uint32_t music_player_story_cd_clock(void){return resource_cdda_clock(&story_cd,music_player_played());}
bool music_player_story_cd_pause(void){
    /* No pending story CD is currently interrupted by a supported room
     * switch. Refuse rather than report a pause while queued DAC audio plays. */
    if(resource_cdda_status(&story_cd,music_player_played())!=0)return false;
    resource_cdda_close(&story_cd);story_cd_track=0;return true;
}
