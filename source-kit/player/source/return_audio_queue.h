#ifndef RETURN_AUDIO_QUEUE_H
#define RETURN_AUDIO_QUEUE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { int16_t *samples; unsigned capacity,read,count; int16_t *const *blocks; unsigned block_frames; } return_audio_queue;
#define RETURN_AUDIO_QUEUE_MAX_FRAMES 65536u
/* Storage belongs to the caller. Single-threaded producer/consumer only. */
bool return_audio_queue_bind(return_audio_queue *,int16_t *,unsigned);
bool return_audio_queue_bind_blocks(return_audio_queue *,int16_t *const *,unsigned,unsigned);
unsigned return_audio_queue_write_view(return_audio_queue *,int16_t **,unsigned);
bool return_audio_queue_publish(return_audio_queue *,unsigned);
unsigned return_audio_queue_read(return_audio_queue *,int16_t *,unsigned);
#endif
