#ifndef RESOURCE_PORTRAIT_HISTORY_H
#define RESOURCE_PORTRAIT_HISTORY_H
#include "resource_live_portrait.h"
#include "resource_opening.h"
/* Immutable runtime source pictures shared by the audio-ahead history.
 * These are transient render results, never recorded game assets. */
typedef struct resource_portrait_picture {
    unsigned references;
    int x;
    uint16_t colors;
    uint16_t source_palette;
    uint64_t identity;
    struct resource_portrait_pattern *pattern;
    struct resource_portrait_picture *next,*previous;
    uint32_t fingerprint;
    uint16_t data[];
} resource_portrait_picture;
typedef struct {
    unsigned count;
    resource_portrait_picture *pictures[3];
} resource_portrait_snapshot;
typedef struct {
    resource_live_portrait_cache cache[3];
    uint16_t scratch[4096];
    unsigned bank;
    uint64_t publication;
    bool initialized;
} resource_portrait_history;
void resource_portrait_snapshot_release(resource_portrait_snapshot *snapshot);
/* Destination and previous must be distinct. Previous remains immutable,
 * including when source RAM or the next snapshot changes. All source ticks
 * are retained by the caller's existing audio history, without pose limits. */
bool resource_portrait_snapshot_take(resource_portrait_history *history,
    resource_portrait_snapshot *destination,const resource_portrait_snapshot *previous,
    const resource_opening_state *video,unsigned bank);
#endif

bool resource_portrait_picture_equal(const resource_portrait_picture *,const uint16_t pixels[4096]);
void resource_portrait_picture_unpack(const resource_portrait_picture *,uint16_t pixels[4096]);
