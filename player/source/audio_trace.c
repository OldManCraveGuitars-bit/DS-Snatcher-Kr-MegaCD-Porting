/* Sampling does no storage I/O and never changes an audio cursor.
 * Keep the latest ~2 minutes; menu-open is the only file-write trigger. */
#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include "music_player.h"
extern volatile unsigned display_frames,shared_game_epoch;
extern volatile unsigned stream_recovery_gaps,stream_recovery_silence,stream_recovery_fault;
extern volatile unsigned audio_refill_calls,audio_refill_samples,audio_refill_low;
enum {TRACE_ROWS=128,TRACE_WORDS=14};
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
}
void audio_trace_write(void){
    const char *drive=fatGetDefaultDrive();if(!drive || !count)return;
    char directory[128],path[160];
    snprintf(directory,sizeof(directory),"%ssnatcher_ds",drive);mkdir(directory,0777);
    snprintf(path,sizeof(path),"%s/audio-performance.txt",directory);
    music_player_service_io();
    FILE *f=fopen(path,"wb");if(!f)return;
    fprintf(f,"Snatcher DS audio-refill v41\n");
    fprintf(f,"display iteration mode scene cue epoch written played gaps silence fault refills refill_samples low_reserve\n");
    unsigned n=count<TRACE_ROWS?count:TRACE_ROWS,start=count-n;
    for(unsigned i=0;i<n;i++){
        const unsigned *p=trace[(start+i)&(TRACE_ROWS-1)];
        if((i&15)==0)music_player_service_io();
        if(fprintf(f,"%u %u %u %04X %02X %u %u %u %u %u %u %u %u %u\n",
            p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],p[12],p[13])<0)break;
    }
    fclose(f);music_player_service_io();
}
