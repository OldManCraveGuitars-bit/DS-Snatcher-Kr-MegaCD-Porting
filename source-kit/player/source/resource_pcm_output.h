#ifndef RESOURCE_PCM_OUTPUT_H
#define RESOURCE_PCM_OUTPUT_H
#include "resource_pcm_chip.h"
#include <stdbool.h>
/* Streaming clock conversion for the PCM device. Linear delta deposition
 * with a DC blocker keeps the source oscillator independent of the DS DAC.
 * Driver/scene ticks must still be scheduled by the caller before rendering
 * audio that passes those event times. */
typedef struct {
    uint64_t factor,next,clocks;
    int32_t previous[2],delta[32][2];
    int64_t integrator[2];
    unsigned cursor,rate;
} resource_pcm_output;
bool resource_pcm_output_reset(resource_pcm_output *output,unsigned rate);
void resource_pcm_output_render_masked(resource_pcm_output *output,resource_pcm_chip *chip,int16_t *stereo,size_t frames,unsigned mask);
void resource_pcm_output_render(resource_pcm_output *output,resource_pcm_chip *chip,int16_t *stereo,size_t frames);
#endif
