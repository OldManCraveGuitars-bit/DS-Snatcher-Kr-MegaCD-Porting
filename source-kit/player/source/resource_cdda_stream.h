#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* A source CDDA stream shares the existing DAC. Decode progress and heard
 * progress are distinct; task14/11 and captions use only the latter. */
typedef struct {
    FILE *file;
    uint32_t frames, produced, origin;
    bool failed;
} resource_cdda_stream;
bool resource_cdda_open(resource_cdda_stream *, const char *, uint32_t origin);
void resource_cdda_close(resource_cdda_stream *);
bool resource_cdda_mix(resource_cdda_stream *, int16_t *, unsigned,
                       int16_t *scratch, unsigned scratch_frames);
bool resource_cdda_position(const resource_cdda_stream *, uint32_t played, unsigned *);
int resource_cdda_status(const resource_cdda_stream *, uint32_t played);
uint32_t resource_cdda_clock(const resource_cdda_stream *, uint32_t played);
