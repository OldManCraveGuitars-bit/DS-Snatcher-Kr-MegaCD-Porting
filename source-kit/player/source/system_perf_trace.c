/* No file I/O while sampling. Explicit Menu L+R+X writes a fixed 128-row snapshot. */
#include <fat.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <string.h>
#include "system_perf.h"
#include "story_audio.h"
#include "music_player.h"
extern volatile unsigned room_tick_debt,room_tick_batch,room_tick_max_batch;
enum {ROWS=128,WORDS=17+PERF_COUNT+4};
static unsigned rows[ROWS][WORDS],count;
void system_perf_sample(const unsigned audio[17]){
    unsigned *p=rows[count++&(ROWS-1)];memcpy(p,audio,17*sizeof(*p));
    system_perf_snapshot(p+17);
    p[17+PERF_COUNT]=room_tick_debt;p[18+PERF_COUNT]=room_tick_batch;
    p[19+PERF_COUNT]=room_tick_max_batch;p[20+PERF_COUNT]=story_audio_running();
}

unsigned system_perf_write_errno;
/* Diagnostic output is explicitly requested. No normal Menu/open-game write.
 * A version-specific name prevents an old log masquerading as a new sample. */
static const char trace_header[]="Snatcher DS v0.95\n";
static char trace_directory[128],trace_path[176],trace_line[384];
static int trace_error(int result){system_perf_write_errno=errno;return result;}
int system_perf_write(void){
    system_perf_write_errno=0;errno=0;
    const char *drive=fatGetDefaultDrive();if(!drive)return trace_error(1);
    if(!count)return trace_error(2);
    int length=snprintf(trace_directory,sizeof(trace_directory),"%ssnatcher_ds",drive);
    if(length<0 || (size_t)length>=sizeof(trace_directory))return trace_error(3);
    if(mkdir(trace_directory,0777) && errno!=EEXIST)return trace_error(4);
    length=snprintf(trace_path,sizeof(trace_path),"%s/system-performance-v095.txt",trace_directory);
    if(length<0 || (size_t)length>=sizeof(trace_path))return trace_error(3);
    music_player_service_io();errno=0;
    FILE *f=fopen(trace_path,"wb");if(!f)return trace_error(5);
    int good=fputs(trace_header,f)>=0 &&
        fputs("Exclusive cumulative time: ticks=1024/BUS_CLOCK seconds; timer3 independent of speech/stream.\n",f)>=0 &&
        fputs("Other includes unclassified work; read=instrumented file reads; IRQ=vblank only. IRQ-masked stalls over ~2s can lose wraps.\n",f)>=0 &&
        fputs("display iteration mode scene cue epoch written played gaps silence fault refills refill_samples low_reserve voice voice_memory voice_refills other audio_overhead fm pcm scene_work draw read worker irq wait room_debt room_batch max_batch speaking\n",f)>=0;
    unsigned n=count<ROWS?count:ROWS,start=count-n;
    for(unsigned i=0;good && i<n;i++){
        const unsigned *p=rows[(start+i)&(ROWS-1)];
        if((i&7)==0)music_player_service_io();
        unsigned used=0;
        for(unsigned j=0;j<WORDS;j++){
            int bytes=snprintf(trace_line+used,sizeof(trace_line)-used,j+1==WORDS?"%u\n":"%u ",p[j]);
            if(bytes<0 || (unsigned)bytes>=sizeof(trace_line)-used){used=0;break;}
            used+=(unsigned)bytes;
        }
        good=used && fwrite(trace_line,1,used,f)==used;
    }
    if(fflush(f) || ferror(f))good=0;
    int saved_errno=errno,closed=fclose(f);
    if(!good || closed){system_perf_write_errno=closed?errno:saved_errno;return 6;}
    music_player_service_io();errno=0;
    /* Confirm this invocation's header, count and final row can be read back.
     * Report success only after buffered writes and close have succeeded. */
    f=fopen(trace_path,"rb");if(!f)return trace_error(7);
    good=fgets(trace_line,sizeof(trace_line),f) && !strcmp(trace_line,trace_header);
    unsigned lines=0;
    while(good && fgets(trace_line,sizeof(trace_line),f)){
        if(!strchr(trace_line,'\n')){good=0;break;}
        lines++;
    }
    if(ferror(f))good=0;
    if(good && lines==n+3){
        unsigned display,iteration;
        good=sscanf(trace_line,"%u %u",&display,&iteration)==2 &&
             display==rows[(count-1)&(ROWS-1)][0] && iteration==rows[(count-1)&(ROWS-1)][1];
    }else good=0;
    saved_errno=errno;closed=fclose(f);music_player_service_io();
    if(!good || closed){system_perf_write_errno=closed?errno:saved_errno;return 7;}
    return 0;
}
void system_perf_summary(char *out,size_t bytes){
    if(count<2){snprintf(out,bytes,"TEST V095\nPLAY FOR 2 SECONDS\n");return;}
    unsigned back=count>5?5:count-1;
    const unsigned *b=rows[(count-1)&(ROWS-1)],*a=rows[(count-1-back)&(ROWS-1)];
    unsigned delta[PERF_COUNT];uint64_t total=0;
    for(unsigned i=0;i<PERF_COUNT;i++){delta[i]=b[17+i]-a[17+i];total+=delta[i];}
    unsigned fm=total?(uint64_t)delta[PERF_FM]*100/total:0;
    unsigned pcm=total?(uint64_t)delta[PERF_PCM]*100/total:0;
    unsigned draw=total?(uint64_t)delta[PERF_DRAW]*100/total:0;
    unsigned read=total?(uint64_t)delta[PERF_READ]*100/total:0;
    unsigned displays=b[0]-a[0],loops=displays?(uint64_t)(b[1]-a[1])*60/displays:0;
    unsigned gaps=b[5]==a[5]?b[8]-a[8]:b[8];
    snprintf(out,bytes,"V095 SCENE %04X\nFM %u%% PCM %u%%\nDRAW %u%% READ %u%%\nLOOP %u GAP %u\n",b[3]&65535,fm,pcm,draw,read,loops>999?999:loops,gaps>9999?9999:gaps);
}
