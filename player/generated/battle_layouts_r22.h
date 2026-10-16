#pragma once
#include <stdint.h>
typedef struct {uint16_t scene,allowed_mask; int16_t x[9],y[9]; uint8_t frame_base[9];} CombatLayout;
static const CombatLayout combat_layouts[] = {
{6400,234,{0,114,0,58,0,180,48,114,0},{0,25,0,53,0,53,103,103,0},{0,7,0,21,0,14,0,0,0}},
{6403,173,{53,0,181,54,0,178,0,114,0},{14,0,14,53,0,60,0,103,0},{14,0,14,14,0,0,0,0,0}},
};
#define COMBAT_LAYOUT_COUNT (sizeof(combat_layouts)/sizeof(combat_layouts[0]))
