#ifndef RESOURCE_SPRITES_H
#define RESOURCE_SPRITES_H
#include "resource_graphics.h"
/* Original ED28/EDA0 actor sprite-map compositor. Original text-helper
 * sprites are separate: NDS dialogue/captions use the lower-screen UI. */
bool resource_sprites_build(const uint8_t actors[64*80],resource_graphics_source source,
    void *context,uint16_t frame,const uint16_t scroll[4],uint8_t sat[64*8],unsigned *count);
/* Optional provenance travels with each emitted SAT record. It does not
 * change native priority, visibility, sprite count or the hardware limit. */
bool resource_sprites_build_owned(const uint8_t actors[64*80],resource_graphics_source source,
    void *context,uint16_t frame,const uint16_t scroll[4],uint8_t sat[64*8],unsigned *count,
    uint8_t owners[64],uint32_t maps[64]);
#endif
