#ifndef VOICE_BUFFERS_H
#define VOICE_BUFFERS_H
#include <stdbool.h>
#include <stdint.h>
#define VOICE_BUFFER_SCRATCH 2048u
typedef struct {void *pcm;int16_t *left,*right;unsigned capacity;} voice_buffers;
bool voice_buffers_begin(voice_buffers *buffers,unsigned capacity,unsigned channels);
bool voice_buffers_reserve(voice_buffers *buffers,unsigned preferred,unsigned minimum,unsigned channels);
/* Last failed allocation: bytes, used, free, top; cumulative failures. */
extern volatile unsigned voice_buffer_failure[5];
void voice_buffers_end(voice_buffers *buffers);
#endif
