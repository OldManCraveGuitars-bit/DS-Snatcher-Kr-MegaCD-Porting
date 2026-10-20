#pragma once
#include <stdint.h>
/* Half-open horizontal spans, one per display row. Empty rows have left>=right. */
typedef struct { uint16_t left,right; } CombatDamageRow;
void combat_damage_rows(const uint32_t before[14],const uint32_t after[14],CombatDamageRow rows[192]);
