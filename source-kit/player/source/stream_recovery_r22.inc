/* Port-local manual PCM starvation recovery. Included inside Maxmod's ARM9
 * stream implementation. Physical DAC time and original source time differ
 * only over explicitly recorded silence gaps; generated samples are retained.
 * This is a playable fallback, NOT proof of original real-time performance. */
#define RECOVERY_TAIL 128u
#define RECOVERY_LEAD 256u
static bool recovery_enabled;
static uint32_t recovery_written, recovery_cleared, recovery_source_written;
static uint32_t recovery_gap_total, recovery_gap_start, recovery_gap_end;
volatile unsigned stream_recovery_gaps, stream_recovery_silence;
volatile unsigned stream_recovery_fault, stream_recovery_max_late;
static mm_word recovery_raw(void);
unsigned port_stream_recovery_error(void) { return recovery_enabled ? stream_recovery_fault : 0; }

static mm_word recovery_position(mm_word physical)
{
    if (!recovery_enabled) return physical;
    unsigned gap = physical <= recovery_gap_start ? 0 :
        physical >= recovery_gap_end ? recovery_gap_end - recovery_gap_start :
        physical - recovery_gap_start;
    unsigned source = physical - recovery_gap_total - gap;
    return source < recovery_source_written ? source : recovery_source_written;
}

/* Clear only retired data, leaving a conservative hardware-start/cache margin.
 * The unwritten part of the looping DAC ring therefore contains silence rather
 * than an earlier bar of music if synthesis misses its deadline. */
static void recovery_clear(unsigned physical)
{
    unsigned replaced = recovery_written > mmsData.length_cut ? recovery_written-mmsData.length_cut : 0;
    if (recovery_cleared < replaced) recovery_cleared = replaced;
    unsigned end = physical > RECOVERY_TAIL ? physical - RECOVERY_TAIL : 0;
    if (end <= recovery_cleared) return;
    unsigned count = end - recovery_cleared, length = mmsData.length_cut;
    if (count > length) { stream_recovery_fault = 1; return; }
    unsigned at = recovery_cleared % length;
    uint16_t *wave = mmsData.wave_memory;
    while (count) {
        unsigned n = count < length-at ? count : length-at;
        memset(wave+at, 0, n*2);
        memset(wave+length+at, 0, n*2);
        DC_FlushRange(wave+at, n*2);
        DC_FlushRange(wave+length+at, n*2);
        count -= n; at = 0;
    }
    recovery_cleared = end;
}

bool port_stream_recovery_enable(unsigned written)
{
    if (recovery_enabled || !mmsData.is_active || mmsData.is_auto ||
        mmsData.format != MM_STREAM_16BIT_STEREO ||
        mmsData.length_cut < 1024) return false;
    unsigned physical = recovery_raw();
    if (physical > written || written-physical >= mmsData.length_cut) return false;
    recovery_written = recovery_source_written = written;
    recovery_cleared = written > mmsData.length_cut ? written-mmsData.length_cut : 0;
    recovery_gap_total = recovery_gap_start = recovery_gap_end = 0;
    stream_recovery_gaps = stream_recovery_silence = stream_recovery_fault = stream_recovery_max_late = 0;
    recovery_clear(physical);
    recovery_enabled = !stream_recovery_fault;
    return recovery_enabled;
}

/* Called at VBlank as well as before/after foreground synthesis. FAT calls
 * may block for much longer than a ring period; the DAC must never replay
 * retired PCM while the foreground is waiting on the card. */
void port_stream_maintenance(void)
{
    unsigned irq=enterCriticalSection();
    if(!recovery_enabled || !mmsData.is_active){leaveCriticalSection(irq);return;}
    StreamCounter+=getAndUpdateStreamPosition(1);
    unsigned physical=recovery_raw();
    recovery_clear(physical);
    if(!stream_recovery_fault && physical>recovery_written){
        unsigned late=physical-recovery_written;
        if(late>stream_recovery_max_late)stream_recovery_max_late=late;
        unsigned end=(physical+RECOVERY_LEAD+3)&~3u;
        if(recovery_written==recovery_gap_end && recovery_gap_end>recovery_gap_start){
            /* No source sample has been committed since the preceding gap.
             * Extend it, rather than counting its silence as source time. */
            stream_recovery_silence+=end-recovery_gap_end;
        }else{
            recovery_gap_total+=recovery_gap_end-recovery_gap_start;
            recovery_gap_start=recovery_written;
            stream_recovery_silence+=end-recovery_gap_start;
            stream_recovery_gaps++;
        }
        recovery_gap_end=end;recovery_written=end;
    }
    leaveCriticalSection(irq);
}

static void recovery_update(void)
{
    port_stream_maintenance();
    if(stream_recovery_fault)return;
    unsigned irq=enterCriticalSection();
    unsigned physical=recovery_raw();
    unsigned wanted=physical+mmsData.length_cut-RECOVERY_TAIL-DELAY_SAMPLES;
    unsigned request=wanted>recovery_written?wanted-recovery_written:0;
    leaveCriticalSection(irq);
    if(request>256)request=256;
    request&=~3u;
    if(!request)return;
    unsigned filled=mmsData.callback(request,mmsData.work_memory,mmsData.format);
    if(filled>request || (filled&3)){stream_recovery_fault=2;return;}
    port_stream_maintenance();
    if(stream_recovery_fault || !filled)return;
    /* The interrupt may retire PCM while the callback runs. Select the
     * current physical destination only after it completes. Copy/publication
     * is one bounded 256-frame transaction; never hold IRQs across synthesis
     * or storage. */
    irq=enterCriticalSection();
    mmsData.position=recovery_written%mmsData.length_cut;
    CopyDataToStream(filled);
    recovery_written+=filled;recovery_source_written+=filled;
    leaveCriticalSection(irq);
}
