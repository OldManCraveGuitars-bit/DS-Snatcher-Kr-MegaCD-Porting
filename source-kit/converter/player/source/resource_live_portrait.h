#ifndef RESOURCE_LIVE_PORTRAIT_H
#define RESOURCE_LIVE_PORTRAIT_H
#include <stdbool.h>
#include <stdint.h>

/* Published original portrait sprites only. No font plane, recorded poses,
 * speaker names or guessed character art. Rectangles retain the source64x64
 * portrait band and are placed in source left-to-right order by the caller. */
typedef struct {
    uint32_t map;
    int16_t x,y;
    uint64_t sprites;
} resource_live_portrait;

bool resource_live_portraits_find(const uint8_t sat[512],unsigned count,
    const uint8_t owners[64],const uint32_t maps[64],
    resource_live_portrait portraits[3],unsigned *found);
bool resource_live_portrait_draw(const resource_live_portrait *portrait,
    const uint8_t sat[512],unsigned count,const uint8_t vram[65536],
    const uint16_t palette[64],uint16_t pixels[4096]);

bool resource_live_portrait_draw_indices(const resource_live_portrait *,
    const uint8_t sat[512],unsigned count,const uint8_t vram[65536],uint8_t pixels[4096]);
void resource_live_portrait_palette(const uint16_t palette[64],uint16_t rgb[64]);

/* Caller resets this on source-bank changes or when reusing the output.
 * Unchanged original poses reuse pixels, never skip source actor ticks. */
typedef struct {
    uint8_t records[512];
    uint16_t palette[64];
    uint32_t transfers,clears,map;
    int16_t x,y;
    unsigned count;
    bool valid;
} resource_live_portrait_cache;
bool resource_live_portrait_draw_cached(resource_live_portrait_cache *cache,
    const resource_live_portrait *portrait,const uint8_t sat[512],unsigned count,
    const uint8_t vram[65536],const uint16_t palette[64],
    uint32_t published_transfers,uint32_t published_clears,
    uint16_t pixels[4096],bool *changed);
#endif
