#pragma once
#include <stdint.h>
#include <stdbool.h>
enum { COMBAT_ACTIVE,COMBAT_WON,COMBAT_LOST };
typedef struct { unsigned phase,cursor,image,clock; int timer; } CombatEnemy;
typedef struct {
    CombatEnemy enemies[9];
    unsigned ticks,kills,shots,misses,damage,delay,weapon,recoil;
    unsigned aim,shot_cell,shot_phase,shot_clock,beam_cell,beam_phase,beam_clock,spawn_phase,spawn_cell;
    int spawn_timer;
    uint16_t random_a,random_b;
    unsigned status,effects;
} CombatState;
extern CombatState combat_state;
bool combat_begin(unsigned stage,unsigned scene,uint32_t seed);
unsigned combat_tick(unsigned aim,bool fire,bool toggle_weapon);
unsigned combat_effects(void);
unsigned combat_sprite_frame(unsigned cell);
int combat_touch_cell(int x,int y);
void combat_render(uint16_t *pixels,const uint16_t *scene,const uint16_t *sprites);
/* Visual state only; timers/RNG continue advancing on every game tick. */
void combat_picture_key(uint32_t key[14]);


#include "combat_damage.h"
void combat_render_rows(uint16_t *,const uint16_t *,const uint16_t *,const CombatDamageRow [192]);

int combat_cell_x(unsigned cell);
int combat_cell_y(unsigned cell);
