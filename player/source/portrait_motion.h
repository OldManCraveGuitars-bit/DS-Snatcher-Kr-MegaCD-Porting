#pragma once
#include "portrait_live.h"
extern bool portrait_live_active;
void portrait_live_mode(bool enabled);
#include <stdint.h>
#include <stdbool.h>
extern unsigned portrait_generation;
bool portrait_motion_begin(unsigned voice);
bool portrait_motion_update(unsigned samples);
bool portrait_motion_idle_tick(void);
bool portrait_story_begin(unsigned voice);
bool portrait_story_update(unsigned samples);
bool portrait_grant_begin(unsigned voice);
bool portrait_grant_update(unsigned samples);
bool portrait_equipment_begin(unsigned voice);
bool portrait_equipment_update(unsigned samples);
bool portrait_dispatch_begin(unsigned voice);
bool portrait_dispatch_update(unsigned samples);
bool portrait_case_begin(unsigned voice);
bool portrait_case_update(unsigned samples);
void portrait_motion_end(void);
const uint16_t *portrait_motion_image(unsigned slot);

bool portrait_combat_story_begin(unsigned voice);
bool portrait_combat_story_update(unsigned samples);
