#pragma once
#include <stdint.h>
/* Store every original row once, before the presenter's vertical scaling.
 * The renderer's192-row staging image repeats original128/160-row artwork.
 * All192 original columns, palette indices and source frames are retained.
 * Both buffers must be word aligned; height is128 or160. */
void resource_frame_store(uint8_t *out,const uint8_t *raster,unsigned height);

void resource_frame_store_native(uint8_t *,const uint8_t *,unsigned);
