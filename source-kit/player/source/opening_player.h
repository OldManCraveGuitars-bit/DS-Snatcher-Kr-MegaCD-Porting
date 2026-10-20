#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "resource_portrait_history.h"
const resource_portrait_snapshot *opening_portraits(void);
void opening_picture_size(unsigned *width,unsigned *height);
bool opening_begin(unsigned stage);
bool opening_credits_begin(uint32_t rng,unsigned saved);
bool opening_skip_to_act1(void);
bool opening_act1_title(void);
void opening_end(void);
bool opening_native_display(void);
bool opening_entry_completed(void);
bool opening_flight_voice(unsigned *samples);
bool opening_forecourt_continue(void);
bool opening_forecourt_restore(void);
/* Keep native scene/sound running while ordinary dialogue and menus own input. */
bool opening_scene_update(void);
// -1: media error, 0: playing, 1: complete. Uses the actual DS sample divisor.
int opening_update(uint16_t pixels[256*192]);
extern unsigned opening_sample;
/* The D1/D0 opening uses an opaque indexed bitmap and original palette. */
bool opening_palette_display(void);
void opening_present_palette(const uint8_t pixels[256*192]);
extern int opening_frame,opening_caption;
extern unsigned opening_stage;

bool opening_return_begin(uint8_t fields[128]);
bool opening_return_finish(uint16_t pixels[256*192]);
