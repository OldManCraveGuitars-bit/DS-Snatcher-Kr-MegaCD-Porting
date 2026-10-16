#ifndef RESOURCE_OPENING_PRESENTATION_H
#define RESOURCE_OPENING_PRESENTATION_H
#include "resource_opening.h"
/* DS presentation only. The original jobs, palette words and clocks stay exact. */
typedef struct {
    uint16_t colors[64],from[64],target[64],elapsed[64],duration[64],fade_span[64],prior_span[64];
    unsigned scene;
    bool initialized;
} resource_opening_presentation;
void resource_opening_presentation_reset(resource_opening_presentation *view);
bool resource_opening_presentation_tick(resource_opening_presentation *view,const resource_opening_state *state);
bool resource_opening_draw_presented(resource_opening_state *state,const resource_opening_presentation *view,uint16_t output[256*192]);
bool resource_opening_draw_presented_indexed_rgb555(resource_opening_state *state,const resource_opening_presentation *view,uint16_t output[256*192]);
bool resource_opening_draw_presented8(resource_opening_state *state,const resource_opening_presentation *view,uint8_t output[256*192]);
#endif
