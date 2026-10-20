#include <nds.h>
#include <filesystem.h>
#include <maxmod9.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include "opening_player.h"
#include "settings.h"
#include "native_startup.h"
#include "native_opening.h"
#include "native_entry.h"
#include "native_entry_display.h"
#include "upper_layout.h"
#include "music_player.h"
#include "video_filters.h"
#include "office_background.h"
#include "reception_background.h"
#include "../generated/opening.h"
#include "../generated/travel.h"
#include "../generated/opening_captions_r22.h"
#include "../generated/cdda_stream_r22.h"
#include "../generated/flight_captions_r22.h"
#include "../generated/return_captions_r22.h"
unsigned opening_sample;
unsigned opening_stage;
volatile unsigned opening_error_state[8];
int opening_frame=-1,opening_caption=-1;
static FILE *audio,*title_audio;
static bool streaming,media_error,entry_display_active,entry_complete;
static bool palette_display_active;
static unsigned audio_left,total_samples,total_captions;
static unsigned stream_rate,stream_format,cdda_frames,travel_prefix;
static unsigned travel_title_frames,travel_entry,travel_ticks,travel_request;
static bool travel_entry_started;
static const uint32_t *starts,*ends;
#ifdef SNATCHER_OPENING_PROFILE
volatile uint32_t opening_audio_profile[3];
#endif
void opening_picture_size(unsigned *w,unsigned *h){
    *w=256;*h=192;
    if(opening_stage>=3){*w=192;*h=128;}
    else if(opening_stage==2){
        /* Copyright ink occupies source rows40..191. Crop only the empty
         * outer rows and keep all glyph rows/columns at native1:1 size. */
        if(opening_frame>=NATIVE_STARTUP_COPYRIGHT_END)*h=224;
    }
    else *h=native_opening_picture_height();
}
static mm_word fill_audio(mm_word length,mm_addr dest,mm_stream_formats format){
#ifdef SNATCHER_OPENING_PROFILE
    uint32_t profile_begin=cpuGetTiming();
    opening_audio_profile[1]+=length;
#endif
    if((unsigned)format!=stream_format){media_error=true;return 0;}
    unsigned wanted=length<audio_left?length:audio_left;
    if(format==MM_STREAM_8BIT_MONO){
        unsigned got=fread(dest,1,wanted,audio);
        if(got!=wanted || ferror(audio))media_error=true;
        audio_left-=got;memset((uint8_t *)dest+got,0,length-got);
    }else{
        unsigned position=total_samples-audio_left,filled=0;
        int16_t *out=dest;
        while(filled<wanted){
            unsigned n=wanted-filled;
            bool original=opening_stage==0 || opening_stage==2 || (opening_stage==1 && position<travel_entry);
            if(original){
                FILE *source=audio;
                unsigned left=cdda_frames-position;
                if(opening_stage==1){
                    if(position<travel_title_frames){source=title_audio;left=travel_title_frames-position;}
                    else left=travel_entry-position;
                }
                if(n>left)n=left;
                if(!n || !source || fread(out+filled*2,4,n,source)!=n || ferror(source)){media_error=true;break;}
            }else{media_error=true;break;}
            filled+=n;position+=n;
        }
        audio_left-=filled;memset(out+filled*2,0,(length-filled)*4);
    }
#ifdef SNATCHER_OPENING_PROFILE
    opening_audio_profile[2]+=cpuGetTiming()-profile_begin;
#endif
    return length;
}
static void end_entry_display(const uint16_t *last_picture){
    native_entry_display_end();entry_display_active=false;
    /* Change bitmap format only after replacing its indexed pixels. Keep
     * the last genuine picture visible while the next scene loads. */
    swiWaitForVBlank();
    int bg=bgInitHidden(3,BgType_Bmp16,BgSize_B16_256x256,0,0);
    bgSetPriority(bg,0);
    if(last_picture){DC_FlushRange(last_picture,256*192*2);dmaCopy(last_picture,BG_GFX,256*192*2);}
    else dmaFillHalfWords(0x8000,BG_GFX,256*192*2);
    dmaFillWords(0,(uint16_t*)BG_GFX+256*192,256*64*2);
    upper_layout_apply_raster(bg,last_picture?192*7:256,last_picture?128*6:192,last_picture?32:0,last_picture?192:256,last_picture?128:192);bgShow(bg);
    video_filters_vblank(true,false);
}
bool opening_native_display(void){return entry_display_active;}
bool opening_palette_display(void){return streaming && opening_stage<=1 && !entry_display_active;}
void opening_present_palette(const uint8_t pixels[256*192]){
    if(!opening_palette_display())return;
    if(!palette_display_active){
        int bg=bgInitHidden(3,BgType_Bmp8,BgSize_B8_256x256,0,0);
        bgSetPriority(bg,0);dmaFillWords(0,(uint8_t*)BG_GFX+256*192,256*64);
        bgShow(bg);palette_display_active=true;
    }
    const uint16_t *palette=native_opening_palette();
    for(unsigned i=0;i<128;i++)BG_PALETTE[128+i]=(settings_tv_filter?video_filters_warm(palette[i&63]):palette[i&63])&32767;
    dmaCopy(pixels,BG_GFX,256*192);
    /* The first indexed frame changes BG3's format; update its TV copy
     * in the same VBlank instead of interpreting indices as RGB words. */
    video_filters_vblank(true,false);
}
bool opening_entry_completed(void){return entry_complete;}
bool opening_forecourt_continue(void){
    if(!entry_display_active || opening_stage!=3 || !native_entry_forecourt_continue())return false;
    opening_stage=4;opening_caption=-1;return true;
}
bool opening_forecourt_restore(void){
    opening_end();office_background_end();reception_background_end();
    if(!music_player_forecourt_begin())return false;
    swiWaitForVBlank();
    int bg=bgInitHidden(3,BgType_Bmp8,BgSize_B8_256x256,0,0);
    if(!native_entry_display_begin(bg,2,1048)){music_player_end();return false;}
    entry_display_active=true;opening_stage=4;opening_caption=opening_frame=-1;return true;
}
bool opening_scene_update(void){
    if(!entry_display_active)return true;
    if(music_player_slice_active())music_player_update_slice();
    else music_player_update();
    int status=native_entry_status();
    return status>=0 && status!=2 && !music_error && !native_entry_display_error();
}
const resource_portrait_snapshot *opening_portraits(void){
    return entry_display_active && opening_stage>=3?native_entry_portraits(music_player_played()):NULL;
}
bool opening_flight_voice(unsigned *samples){
    if(opening_stage!=3 || !entry_display_active || !samples)return false;
    unsigned start=native_entry_voice_begin(),end=native_entry_voice_end(),played=music_player_played();
    if(!start || played<start || (end && played>=end))return false;
    *samples=(uint64_t)(played-start)*16277/32000;return true;
}
void opening_end(void){
    if(entry_display_active){end_entry_display(NULL);music_player_end();}
    if(palette_display_active){end_entry_display(NULL);palette_display_active=false;}
    if(streaming)mmStreamClose();
    streaming=false;
    if(audio)fclose(audio);
    audio=NULL;
    if(title_audio)fclose(title_audio);
    title_audio=NULL;
    native_startup_end();
    native_opening_end();
}
static bool read_stream_header(FILE *f,unsigned *frames){
    uint8_t h[24];
    if(!f || fread(h,1,24,f)!=24 || memcmp(h,"NST1\1\0\2\0",8) ||
       memcmp(h+8,"\0\175\0\0",4) || memcmp(h+16,"\30\0\0\0\20\0\0\0",8))return false;
    *frames=h[12]|(unsigned)h[13]<<8|(unsigned)h[14]<<16|(unsigned)h[15]<<24;
    return *frames && *frames<=32000u*1800u;
}
bool opening_begin(unsigned stage){
    reception_background_end();opening_end();music_player_end();entry_complete=false;opening_frame=opening_caption=-1;opening_sample=0;
    office_background_end();reception_background_end();
    if(stage>3)return false;
    opening_stage=stage;media_error=false;
    total_samples=stage?TRAVEL_SAMPLES:OPENING_SAMPLES;
    total_captions=stage?NATIVE_TRAVEL_CAPTIONS:OPENING_CAPTIONS;
    starts=stage?native_travel_caption_start:opening_caption_start;ends=stage?native_travel_caption_end:opening_caption_end;
    if(stage==2 || stage==3)total_captions=0;
    stream_rate=OPENING_RATE;stream_format=MM_STREAM_8BIT_MONO;cdda_frames=travel_prefix=0;
    travel_title_frames=travel_entry=travel_ticks=travel_request=0;travel_entry_started=false;
    if(!nitroFSInit(NULL))return false;
    if(stage==3){
        if(!music_player_flight_begin())return false;
        struct mallinfo heap=mallinfo();
        opening_error_state[4]=heap.fordblks;opening_error_state[5]=heap.uordblks;
        swiWaitForVBlank();
        int bg=bgInitHidden(3,BgType_Bmp8,BgSize_B8_256x256,0,0);
        if(!native_entry_display_begin(bg,2,1048)){music_player_end();return false;}
        entry_display_active=true;return true;
    }
    {
        audio=fopen(stage==2?"nitro:/native-boot-audio-r22.nsta":stage?"nitro:/native-cdda-04-r22.nsta":"nitro:/native-cdda-03-r22.nsta","rb");
        if(!audio ||
           (stage==2 && !native_startup_begin()) || (stage==0 && !native_opening_begin(0)) ||
           (stage==1 && !native_opening_begin_track(4,0))){opening_end();return false;}
        if(stage<=2){
            if(!read_stream_header(audio,&cdda_frames)){
                opening_end();return false;
            }
            stream_rate=32000;stream_format=MM_STREAM_16BIT_STEREO;
            if(stage==0 || stage==2)total_samples=cdda_frames;
            else{
                travel_prefix=TRAVEL_CDDA_PREFIX_16K*2;
                title_audio=fopen("nitro:/native-cdda-05-r22.nsta","rb");
                if(!read_stream_header(title_audio,&travel_title_frames)){opening_end();return false;}
                travel_entry=travel_title_frames+cdda_frames;
                total_samples=travel_entry;
            }
        }
        if(!music_player_init()){opening_end();return false;}
        audio_left=total_samples;
        mm_stream stream={.sampling_rate=stream_rate,.buffer_length=16384,.callback=fill_audio,
            .format=stream_format,.timer=MM_TIMER2,.manual=true};
        mmStreamOpen(&stream);streaming=true;mmStreamVolume(settings_volume());
        if(media_error){opening_end();return false;}
        return true;
    }
}
int opening_update(uint16_t pixels[256*192]){
    if(entry_display_active){
        if(music_player_slice_active())music_player_update_slice();
        else music_player_update();
        unsigned played=music_player_played();
        int status=native_entry_status();opening_caption=-1;
        unsigned voice_sample;
        if(opening_flight_voice(&voice_sample))opening_caption=0;
        else if(opening_stage==3 && music_player_flight_cdda_sample(&voice_sample)){
            for(unsigned i=0;i<FLIGHT_CDDA_CAPTIONS;i++)
                if(voice_sample>=flight_cdda_caption_start[i] && voice_sample<flight_cdda_caption_end[i]){opening_caption=(int)i+1;break;}
        }
        if(opening_stage==5){
            unsigned voice=native_entry_return_voice(played);
            if(voice>=170 && voice<=180)opening_caption=voice-170;
            else if(music_player_flight_cdda_sample(&voice_sample))
                for(unsigned i=0;i<RETURN_CDDA_CAPTIONS;i++)
                    if(voice_sample>=return_cdda_start_sample[i] && voice_sample<return_cdda_end_sample[i]){opening_caption=11+i;break;}
        }
        opening_frame=native_entry_display_sample();
        if(status<0 || status==2 || music_error || native_entry_display_error()){
            opening_error_state[0]=status;opening_error_state[1]=music_error;
            opening_error_state[2]=native_entry_display_error();opening_error_state[3]=native_entry_written();
            opening_error_state[6]=played;opening_end();return -1;
        }
        if(status==1 && played>=native_entry_handoff_sample()){
            if(opening_stage>=3){
                /* The dialogue boundary is fixed. The device clock and
                 * indexed presenter stay live while the user reads. */
                unsigned expected=opening_stage==5?0x1b1:opening_stage==3?0x18:0x97;
                return native_entry_handoff()==expected?1:-1;
            }
            if(!native_entry_draw(played,pixels)){opening_end();return -1;}
            /* The final played picture is now in pixels and VRAM. The IRQ
             * must stop before releasing its already-consumed history. */
            native_entry_display_end();
            if(!native_entry_release_played_history()){opening_end();return -1;}
            if(!reception_background_adopt_entry()){opening_end();return -1;}
            end_entry_display(pixels);
            if(!music_player_entry_release()){music_player_end();return -1;}
            if(!reception_background_rebind()){opening_end();return -1;}
            entry_complete=true;return 1;
        }
        return 0;
    }
#ifdef SNATCHER_OPENING_PROFILE
    uint32_t profile_begin=cpuGetTiming();
    opening_audio_profile[1]=opening_audio_profile[2]=0;
#endif
    mmStreamUpdate();unsigned elapsed=mmStreamGetPosition();
#ifdef SNATCHER_OPENING_PROFILE
    opening_audio_profile[0]=cpuGetTiming()-profile_begin;
#endif
    opening_sample=(uint64_t)elapsed*OPENING_RATE/stream_rate;
    if(media_error){opening_end();return -1;}
    if(elapsed>=total_samples){
        if(opening_stage==1){
            if(!native_opening_finished()){opening_end();return -1;}
            opening_end();
            if(!music_player_entry_begin())return -1;
            int bg=bgInitHidden(3,BgType_Bmp8,BgSize_B8_256x256,0,0);
            if(!native_entry_display_begin(bg,2,1048)){music_player_end();return -1;}
            entry_display_active=travel_entry_started=true;opening_caption=-1;opening_frame=-1;
            return 0;
        }
        opening_end();return 1;
    }
    if(opening_stage==2){
        unsigned target=(uint64_t)elapsed*53693175/((uint64_t)32000*896040);
        if(target>=NATIVE_STARTUP_FRAMES)target=NATIVE_STARTUP_FRAMES-1;
        if((int)target!=opening_frame){
            if(!native_startup_draw(target,pixels)){opening_end();return -1;}
            opening_frame=target;
        }
        return 0;
    }
    if(opening_stage==1 && elapsed>=travel_title_frames)
        opening_sample=(travel_prefix+elapsed-travel_title_frames)/2;
    opening_caption=-1;
    for(unsigned i=0;i<total_captions;i++)if(opening_sample>=starts[i] && opening_sample<ends[i]){opening_caption=i;break;}
    if(opening_stage==0){
        unsigned target=(uint64_t)elapsed*53693175/((uint64_t)32000*896040);
        if((int)target!=opening_frame){
            if(!native_opening_draw(target,(uint8_t*)pixels)){opening_end();return -1;}
            opening_frame=target;
        }
        return 0;
    }
    if(opening_stage==1 && elapsed<travel_entry){
        unsigned target=(uint64_t)elapsed*53693175/((uint64_t)32000*896040);
        if((int)target!=opening_frame){
            while(travel_ticks<=target){
                unsigned sample=(uint64_t)travel_ticks*32000*896040/53693175;
                unsigned track=sample<travel_title_frames?5:4;
                if(track==4 && travel_request!=4){opening_end();return -1;}
                unsigned clock_sample=track==5?sample:sample-travel_title_frames;
                unsigned status=travel_request==track?0x100:0;
                if(!native_opening_tick_cd((uint64_t)clock_sample*75/32000,status)){opening_end();return -1;}
                unsigned request=native_opening_take_cd_request();
                if(request){
                    if((request!=4 && request!=5) || (request==5 && travel_request) || (request==4 && travel_request!=5)){
                        opening_end();return -1;
                    }
                    travel_request=request;
                }
                travel_ticks++;
            }
            if(!native_opening_draw_current((uint8_t*)pixels)){opening_end();return -1;}
            opening_frame=target;
        }
        return 0;
    }
    opening_end();return -1;
}

bool opening_return_begin(uint8_t fields[128]){
    if(entry_display_active || streaming || !reception_background_begin_return(fields))return false;
    swiWaitForVBlank();int bg=bgInitHidden(3,BgType_Bmp8,BgSize_B8_256x256,0,0);
    if(!native_entry_display_begin_live(bg,2,1048,music_player_played()))return false;
    entry_display_active=true;opening_stage=5;opening_frame=opening_caption=-1;return true;
}
volatile unsigned opening_return_fault;
bool opening_return_finish(uint16_t pixels[256*192]){
    opening_return_fault=1;
    if(opening_stage!=5 || !entry_display_active)return false;
    /* The indexed compositor is exclusively owned by the VBlank presenter.
     * Stop it before a main-thread final draw can race a newer source frame. */
    native_entry_display_end();
    opening_return_fault=2;
    if(!native_entry_draw(music_player_played(),pixels))return false;
    opening_return_fault=3;
    if(!native_entry_return_release_history())return false;
    opening_return_fault=4;
    if(!reception_background_finish_return())return false;
    end_entry_display(pixels);
    opening_return_fault=5;
    if(!music_player_return_release())return false;
    opening_return_fault=6;
    if(!reception_background_rebind())return false;
    opening_return_fault=0;return true;
}
