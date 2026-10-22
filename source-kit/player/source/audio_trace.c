#include "system_perf.h"
/* Sampling does no storage I/O and never changes an audio cursor.
 * Keep the latest ~2 minutes; an explicit Menu L+R+X chord is the only file-write trigger. */
#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include "music_player.h"
extern volatile unsigned display_frames,shared_game_epoch;
extern volatile unsigned stream_recovery_gaps,stream_recovery_silence,stream_recovery_fault;
extern volatile unsigned audio_refill_calls,audio_refill_samples,audio_refill_low;
static const unsigned story_voice_memory_bytes=0;
extern unsigned story_audio_voice;
extern volatile unsigned story_stream_diagnostic[6];
enum {TRACE_ROWS=128,TRACE_WORDS=17};
static unsigned trace[TRACE_ROWS][TRACE_WORDS],count,last;
void audio_trace_sample(unsigned mode,unsigned scene,unsigned iterations){
    unsigned now=display_frames;
    if(count && now-last<60)return;
    unsigned *p=trace[count++&(TRACE_ROWS-1)];last=now;
    p[0]=now;p[1]=iterations;p[2]=mode;p[3]=scene;p[4]=music_command;
    p[5]=shared_game_epoch;p[6]=music_written;p[7]=music_player_played();
    p[8]=stream_recovery_gaps;p[9]=stream_recovery_silence;
    p[10]=music_error|(stream_recovery_fault<<16);
    p[11]=audio_refill_calls;p[12]=audio_refill_samples;p[13]=audio_refill_low;
    p[14]=story_audio_voice;p[15]=story_voice_memory_bytes;p[16]=story_stream_diagnostic[5];
    system_perf_sample(p);
}

volatile unsigned audio_trace_write_calls;
volatile int audio_trace_write_result;
void audio_trace_write(void){
    audio_trace_write_calls++;
    /* The combined record includes every audio field; one write is enough. */
    audio_trace_write_result=system_perf_write();
}
