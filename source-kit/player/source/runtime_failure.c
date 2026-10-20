/* Write one bounded diagnostic only after playback has failed. Never touch slots. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <fat.h>
#include <sys/stat.h>
#include "music_player.h"
#include "story_vm.h"
#include "opening_player.h"
extern volatile unsigned display_frames,fm_error,fm_compact_fault,fm_stack_bad;
extern volatile unsigned fm_failed_allocation,fm_allocation_failures;
extern volatile int music_min_fill_lead;
extern volatile unsigned opening_error_state[8],native_entry_diagnostic[8],reception_error_state[9];
extern volatile int32_t resource_storage_state[6];
extern void music_player_halt_failed(void);
extern volatile unsigned shared_played_fault,stream_recovery_fault,stream_recovery_gaps,stream_recovery_silence,stream_recovery_max_late;
static unsigned recorded;
void runtime_failure_record(unsigned kind,unsigned mode,unsigned scene){
    if(recorded)return;
    recorded=1;
    /* Stop replaying stale DAC-ring data. Keep driver/graphics state intact
     * so the failure can be inspected and the existing load menu can recover. */
    unsigned played=music_player_played();
    music_player_halt_failed();
    struct mallinfo heap=mallinfo();
    char directory[128],path[160];
    const char *drive=fatGetDefaultDrive();if(!drive)return;
    snprintf(directory,sizeof(directory),"%ssnatcher_ds",drive);mkdir(directory,0777);
    snprintf(path,sizeof(path),"%s/runtime-error.txt",directory);
    FILE *f=fopen(path,"wb");if(!f)return;
    fprintf(f,"Snatcher DS chapter-select v62\nkind=%u mode=%u scene=%04x display=%u\n",kind,mode,scene,display_frames);
    fprintf(f,"story pc=%04x op=%02x arg=%08x\n",story_event_pc,story_event_op,story_event_arg);
    fprintf(f,"opening stage=%u frame=%d sample=%u\n",opening_stage,opening_frame,opening_sample);
    fprintf(f,"audio error=%u missing=%u cue=%02x written=%u played=%u lead=%d fill_lead=%d\n",music_error,music_missing,music_command,music_written,played,music_min_lead,music_min_fill_lead);
    fprintf(f,"fm error=%u fault=%u stack=%u allocation=%u failures=%u\n",fm_error,fm_compact_fault,fm_stack_bad,fm_failed_allocation,fm_allocation_failures);
    fprintf(f,"recovery fault=%u shared=%u gaps=%u silence=%u late=%u\n",stream_recovery_fault,shared_played_fault,stream_recovery_gaps,stream_recovery_silence,stream_recovery_max_late);
    fprintf(f,"heap used=%d free=%d\n",heap.uordblks,heap.fordblks);
    fprintf(f,"opening:");for(unsigned i=0;i<8;i++)fprintf(f," %u",opening_error_state[i]);
    fprintf(f,"\nentry:");for(unsigned i=0;i<8;i++)fprintf(f," %u",native_entry_diagnostic[i]);
    fprintf(f,"\nroom:");for(unsigned i=0;i<9;i++)fprintf(f," %u",reception_error_state[i]);
    fprintf(f,"\nstorage:");for(unsigned i=0;i<6;i++)fprintf(f," %ld",(long)resource_storage_state[i]);
    fprintf(f,"\n");fclose(f);
}
void runtime_failure_reset(void){recorded=0;}
