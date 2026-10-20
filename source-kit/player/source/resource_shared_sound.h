#ifndef RESOURCE_SHARED_SOUND_H
#define RESOURCE_SHARED_SOUND_H
#include "resource_room_audio.h"

/* One original MAIN FM driver, six YM channels and the original SUB PCM
 * device. The owner supplies monotonically increasing DAC sample positions;
 * rendering ahead never consumes a played-time completion notification. */
typedef struct {
    uint32_t frame,pcm;
    uint8_t signal,music_level;
} resource_sound_notification;
/* One notification per native frame. Retain the full finite sound
 * lead, including its boundary frame; never discard completions. */
#define RESOURCE_SHARED_SOUND_NOTICES 128u
#define RESOURCE_SHARED_SOUND_NOTICE_MASK (RESOURCE_SHARED_SOUND_NOTICES-1)
typedef struct {
    resource_room_audio pcm;
    resource_sound_notification notices[RESOURCE_SHARED_SOUND_NOTICES],pending_pcm[4];
    uint64_t samples,played;
    uint32_t next_frame,generated_frame,music_request,effect_request;
    uint16_t notice_read,notice_count,pcm_read,pcm_count;
    uint8_t music,effect,sequence,error;
    bool initialized;
} resource_shared_sound;

bool resource_shared_sound_begin(resource_shared_sound *,const resource_pcm_effect_bank *,
    const void *ip,unsigned ip_bytes,const void *fm,unsigned fm_bytes);
void resource_shared_sound_end(resource_shared_sound *);
bool resource_shared_sound_request(resource_shared_sound *,unsigned frame,unsigned command,unsigned channel);
bool resource_shared_sound_render(resource_shared_sound *,int16_t *,unsigned samples);
/* Accumulate the complete mixed-output hash in the same pass as PCM mixing. */
bool resource_shared_sound_render_observed(resource_shared_sound *,int16_t *,unsigned samples,uint32_t *hash);
void resource_shared_sound_generated_status(const resource_shared_sound *,uint8_t *,uint8_t *);
bool resource_shared_sound_played(resource_shared_sound *,uint64_t samples);
uint64_t resource_shared_sound_boundary(unsigned source_frame);
/* Checkpoints include every generated-but-unplayed stereo frame. The caller
 * supplies that exact [played,samples) FIFO and must re-prime it after import.
 * Wire bytes are portable; no callback, pointer or ABI padding is serialized.
 * Import is transactional. State and all source/destination buffers are disjoint.
 * This device codec does not by itself connect the game's five save slots. */
#define RESOURCE_SHARED_SOUND_AHEAD_MAX 65536u
unsigned resource_shared_sound_export(const resource_shared_sound *,const int16_t *unplayed,
    unsigned unplayed_frames,void *data,unsigned capacity);
bool resource_shared_sound_import(resource_shared_sound *,const void *data,unsigned bytes,
    int16_t *unplayed,unsigned capacity_frames,unsigned *unplayed_frames);
#endif
