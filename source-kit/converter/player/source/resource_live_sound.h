#ifndef RESOURCE_LIVE_SOUND_H
#define RESOURCE_LIVE_SOUND_H
#include <stdbool.h>
#include <stdint.h>
/* Source ticks and communication values, not look-ahead playback completion.
 * The presenter must consume these at the actual played sample boundary. */
typedef void (*resource_live_sound_observer)(void *,unsigned,uint8_t,uint32_t);
bool forecourt_fm_begin_shared(const void *,unsigned,const void *,unsigned);
bool forecourt_fm_schedule(unsigned source_frame,unsigned command);
bool forecourt_fm_request_sub(unsigned source_frame,unsigned command,unsigned channel);
unsigned forecourt_fm_music_level(void);
void forecourt_fm_observe(resource_live_sound_observer,void *);
#endif
