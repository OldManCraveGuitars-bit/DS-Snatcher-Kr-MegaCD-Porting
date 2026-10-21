#pragma once
#include <stdbool.h>
extern bool settings_buttons;
extern bool settings_scanlines;
extern bool settings_tv_filter;
extern unsigned settings_level;
extern bool settings_bgm,settings_effects,settings_voice;
/* Output gates only: never stop a source clock or completion notification. */
unsigned settings_pcm_mask(bool speech);
unsigned settings_fm_mask(unsigned original_effect_flags);
unsigned settings_voice_volume(void);
void settings_load(void);
bool settings_store(void);
unsigned settings_volume(void);
