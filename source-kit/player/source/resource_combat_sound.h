#ifndef RESOURCE_COMBAT_SOUND_H
#define RESOURCE_COMBAT_SOUND_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Stage-indexed commands from original SUB tables. No wave/score payload. */
typedef struct { uint8_t hit,shot,enemy,draw,damage; } resource_combat_sounds;
bool resource_combat_sound_bind(resource_combat_sounds *out,const void *sub,size_t bytes,unsigned stage);
#endif
