#ifndef RESOURCE_ROOM_AUDIO_H
#define RESOURCE_ROOM_AUDIO_H
#include "resource_pcm_driver.h"
#include "resource_pcm_output.h"
typedef struct {
    resource_pcm_driver driver;
    resource_pcm_output output;
    uint32_t samples,ticks,next_tick;
    const uint8_t *drum_commands;
    uint32_t drum_count,drum_loop_start,drum_sequence_delta;
    uint32_t drum_samples,drum_ticks,next_drum_tick,drum_loops;
} resource_room_audio;
bool resource_room_audio_bind(resource_room_audio *state,const resource_pcm_effect_bank *bank);
bool resource_room_audio_render(resource_room_audio *state,int16_t *stereo,unsigned frames);
/* Original packed drum commands; input remains owned by the caller. The
 * RF5C164 oscillator/filter continue across the original score repeats. */
bool resource_room_audio_drums_begin(resource_room_audio *state,const void *data,size_t bytes);
void resource_room_audio_drums_end(resource_room_audio *state);
#endif
