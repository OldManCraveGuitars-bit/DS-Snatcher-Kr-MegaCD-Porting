#ifndef RESOURCE_MUSIC_H
#define RESOURCE_MUSIC_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
/* Converted original score/instruments. PCM is interleaved signed16 stereo.
 * The end sample is exclusive; repeat never inserts silence or repeats intro.
 * UINT32_MAX loop_start denotes a finite score. Only an independently proven
 * stopped score may set ended_silence; other finite exhaustion fails. */
typedef struct {
    FILE *file;
    unsigned frames,loop_start,loop_end,position,loops,command;
    bool error,ended_silence;
    uint8_t *buffer;
    struct resource_music_channels *channels;
} resource_music;
bool resource_music_open(resource_music *s,const char *path,unsigned command);
void resource_music_close(resource_music *s);
bool resource_music_read(resource_music *s,int16_t *out,unsigned frames);
#endif
