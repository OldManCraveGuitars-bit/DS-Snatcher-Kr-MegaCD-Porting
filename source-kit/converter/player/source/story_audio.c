#include <nds.h>
#include <filesystem.h>
#include <stdio.h>
extern size_t audio_serviced_fread(void *,size_t,size_t,FILE *);
#include <stdlib.h>
#include <malloc.h>
#include "resource_voice_stream.h"
#include "music_player.h"
#include "story_audio.h"
#include "portrait_motion.h"
#include "reception_background.h"
#include "settings.h"
#include "native_entry.h"
#include "../generated/story_voices.h"
#include "../generated/grant_audio.h"
#include "../generated/equipment_audio.h"
#include "../generated/dispatch_audio.h"
#include "../generated/case_audio.h"
#include "../generated/combat_story_audio.h"
#include "../generated/headquarters_audio.h"
#include "../generated/contact_audio.h"
#include "../generated/voice_audio_r22.h"
static void *pcm;
static FILE *voice_file;
static bool voice_live_stream;
static resource_voice_stream voice_stream;
#define VOICE_RING_BYTES 32768u
volatile unsigned story_stream_diagnostic[6];
static bool voice_read(void *context,void *output,size_t bytes){
    FILE *f=context;
    /* Initialization may service the complete BGM backlog. A live voice
     * refill is only one 2KiB read: keep its audio service bounded so it
     * cannot turn a cheap foreground read into a long synthesis loop. */
    if(!voice_live_stream)return audio_serviced_fread(output,1,bytes,f)==bytes && !ferror(f);
    if(music_player_slice_active())music_player_update_slice();
    else music_player_update();
    return fread(output,1,bytes,f)==bytes && !ferror(f);
}
static void voice_publish(void *context,void *output,size_t bytes){
    (void)context;DC_FlushRange(output,bytes);
}
static unsigned length,divisor;
static int channel=-1;
static bool running,owned,prepared,native_voice;
bool story_audio_caption_active;
bool story_audio_uses_source_clock(void){return native_voice;}
unsigned story_audio_voice;
void story_audio_end(void){
    if(running && !native_voice)cpuEndTiming();
    if(native_voice)native_entry_story_voice_release();
    native_voice=false;story_audio_caption_active=false;
    running=false;voice_live_stream=false;
    if(channel>=0)soundKill(channel);
    channel=-1;
    portrait_motion_end();
    if(owned)free((void *)pcm);
    pcm=NULL;owned=prepared=false;
    if(voice_file)fclose(voice_file);
    voice_file=NULL;voice_stream.buffer=NULL;
}
bool story_audio_prepare(unsigned voice){
    if(running)return false;
    story_audio_end();
    portrait_live_mode(reception_background_active() || native_entry_external_voice_ready());
    if(native_entry_external_voice_ready()){
        if(!native_entry_prepare_voice(voice))return false;
        story_audio_voice=voice;native_voice=prepared=true;return true;
    }
    int contact=contact_voice_index(voice);
    if(voice!=1 && voice!=2 && voice!=5 && voice!=6 && voice!=7 && voice!=8 && voice!=9 && voice!=21 && voice!=28 && voice!=16 && voice!=20 && voice!=32 && voice!=37 && voice!=40 && ((voice<77 || voice>169) && (voice<181 || voice>201)) && contact<0)return false;
    if(!nitroFSInit(NULL))return false;
    char path[48];snprintf(path,sizeof(path),"nitro:/pcm%04u.bin",voice);
    FILE *f=fopen(path,"rb");if(!f)return false;
    bool good=!fseek(f,0,SEEK_END);long bytes=good?ftell(f):-1;
    good=good && bytes>0 && bytes<=786432 && !(bytes&3) && !fseek(f,0,SEEK_SET);
    voice_file=f;
    if(good){
        owned=true;pcm=memalign(32,VOICE_RING_BYTES);
        good=pcm && resource_voice_stream_begin(&voice_stream,pcm,VOICE_RING_BYTES,bytes,voice_read,voice_publish,f);
    }
    story_stream_diagnostic[0]=voice;story_stream_diagnostic[1]=VOICE_RING_BYTES;
    story_stream_diagnostic[2]=bytes>0?(unsigned)bytes:0;
    story_stream_diagnostic[3]=voice_stream.error;
    story_stream_diagnostic[4]=voice_stream.min_lead;story_stream_diagnostic[5]=0;
    if(!good){story_audio_end();return false;}
    if(voice>=77 && voice<=85 && ((unsigned)bytes!=story_voice_sizes[voice-77] || (!portrait_live_active && !portrait_story_begin(voice)))){story_audio_end();return false;}
    if(voice>=86 && voice<=94 && ((unsigned)bytes!=voice_sizes[voice-86] || (!portrait_live_active && !portrait_motion_begin(voice-86)))){story_audio_end();return false;}
    if(voice>=95 && voice<=102 && ((unsigned)bytes!=grant_voice_sizes[voice-95] || (!portrait_live_active && !portrait_grant_begin(voice)))){story_audio_end();return false;}
    if(voice>=103 && voice<=120 && ((unsigned)bytes!=equipment_voice_sizes[voice-103] || (!portrait_live_active && !portrait_equipment_begin(voice)))){story_audio_end();return false;}
    if(voice>=121 && voice<=136 && ((unsigned)bytes!=dispatch_voice_sizes[voice-121] || (!portrait_live_active && !portrait_dispatch_begin(voice)))){story_audio_end();return false;}
    if(voice>=137 && voice<=143 && ((unsigned)bytes!=case_voice_sizes[voice-137] || (!portrait_live_active && !portrait_case_begin(voice)))){story_audio_end();return false;}
    if(voice>=144 && voice<=169 && ((unsigned)bytes!=combat_story_voice_sizes[voice-144] || (!portrait_live_active && !portrait_combat_story_begin(voice)))){story_audio_end();return false;}
    if(voice>=181 && voice<=201 && (!portrait_live_active || (unsigned)bytes!=headquarters_voice_sizes[voice-181])){story_audio_end();return false;}
    if(contact>=0 && (!portrait_live_active || (unsigned)bytes!=contact_voice_sizes[contact])){story_audio_end();return false;}
    story_audio_voice=voice;divisor=((BUS_CLOCK/2)+(STORY_VOICE_RATE/2))/STORY_VOICE_RATE;length=bytes*2*divisor;
    prepared=true;return true;
}
bool story_audio_start(void){
    if(native_voice){
        if(!prepared || running || !native_entry_start_voice(story_audio_voice))return false;
        running=true;story_audio_caption_active=false;return true;
    }
    if(!prepared || !pcm || running)return false;
    channel=soundPlaySampleChannel(0,pcm,SoundFormat_8Bit,VOICE_RING_BYTES,STORY_VOICE_RATE,settings_volume(),64,true,0);
    if(channel<0){story_audio_end();return false;}
    cpuStartTiming(0);running=voice_live_stream=story_audio_caption_active=true;return true;
}
bool story_audio_begin(unsigned voice){return story_audio_prepare(voice) && story_audio_start();}
bool story_audio_running(void){return running;}
unsigned story_audio_samples(void){
    if(native_voice){unsigned sample=0;bool audible;native_entry_story_voice(music_player_played(),&sample,&audible);return sample;}
    return running?cpuGetTiming()/(2*divisor):0;
}
int story_audio_update(void){
    if(!running)return 0;
    if(native_voice){
        unsigned samples;bool audible;
        int result=native_entry_story_voice(music_player_played(),&samples,&audible);
        story_audio_caption_active=audible;
        if(result<=0)story_audio_end();
        return result;
    }
    unsigned ticks=cpuGetTiming();
    bool supplied=resource_voice_stream_update(&voice_stream,ticks/(2*divisor));
    story_stream_diagnostic[3]=voice_stream.error;
    story_stream_diagnostic[4]=voice_stream.min_lead;
    story_stream_diagnostic[5]=voice_stream.refills;
    if(!supplied){story_audio_end();return -1;}
    ticks=cpuGetTiming();
    unsigned voice=story_audio_voice,samples=ticks/(2*divisor);
    bool good=portrait_live_active || voice<77 || (voice<=85?portrait_story_update(samples):voice<=94?portrait_motion_update(samples):voice<=102?portrait_grant_update(samples):voice<=120?portrait_equipment_update(samples):voice<=136?portrait_dispatch_update(samples):voice<=143?portrait_case_update(samples):portrait_combat_story_update(samples));
    if(!good){story_audio_end();return -1;}
    if(ticks>=length){story_audio_end();return 0;}return 1;
}
