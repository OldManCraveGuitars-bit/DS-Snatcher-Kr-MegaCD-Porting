#ifndef RESOURCE_SCENE_H
#define RESOURCE_SCENE_H
#include "resource_actor.h"
#include "resource_palette.h"
#include "resource_graphics.h"
#include "resource_scroll.h"
/* Resource scene packets, deferred tile loads and native effect queues.
 * Bind only original extracted data; caller owns the input lifetime. */
typedef struct {
    const uint8_t *resource,*subcode;
    size_t resource_bytes,subcode_bytes;
    uint8_t globals[128]; /* 7860..78DF */
    uint8_t actors[64*80];
    resource_palette_state palette;
    resource_graphics_state graphics;
    resource_scroll_state scroll;
    /* Native publication compares4-byte pattern rows; keep the port-owned
     * VRAM page aligned even though preceding native control fields are bytes. */
    uint8_t vram[65536] __attribute__((aligned(4)));
    uint8_t shared_ready,error;
    uint32_t transfers;
    uint16_t display_mode; /* pending Main B6406 plane-size request */
    uint16_t picture_mode; /* SUB79EE contact/backup presentation state */
    uint32_t written_tiles[64]; /* Host publication bookkeeping, not native state. */
} resource_scene_state;
bool resource_scene_bind(resource_scene_state *state,const void *resource,size_t resource_bytes,const void *subcode,size_t subcode_bytes);
bool resource_scene_load(resource_scene_state *state,unsigned scene);
bool resource_scene_popup(resource_scene_state *state,unsigned picture,unsigned position);
bool resource_scene_clear(resource_scene_state *state);
void resource_scene_clear_voice_actors(resource_scene_state *state);
/* Original F600..F640 restores the shared first eight colours. Room
 * contexts inherit these from boot even when their own packets omit them. */
bool resource_scene_shared_palette(resource_scene_state *state);
bool resource_scene_blackout(resource_scene_state *state);
bool resource_scene_tiles_tick(resource_scene_state *state,bool dma_busy);
/* Native effect callback used by resource_actor_tick_effects. */
bool resource_scene_actor_effect(void *context,unsigned actor,unsigned opcode,unsigned arg,uint16_t value,uint32_t sequence);
bool resource_scene_actors_tick(resource_scene_state *state,uint8_t pcm_flags);
/* External registers used by the original actor commands. Sound requests go
 * to the real driver in instruction order; an absent driver is an error, not
 * a successful silent command. The callback may update pcm_flags (1511E)
 * before a later actor/command tests the same register during this tick. */
typedef struct resource_scene_devices {
    uint16_t frame_clock; /* 784A */
    uint8_t pcm_flags;    /* 1511E */
    uint8_t music;        /* 980E */
    uint8_t effect;       /* 980F */
    void *context;
    bool (*sound)(void *context,uint32_t entry,unsigned argument,unsigned channel,
                  struct resource_scene_devices *devices);
} resource_scene_devices;
bool resource_scene_actors_tick_devices(resource_scene_state *state,resource_scene_devices *devices);
const uint8_t *resource_scene_resolve(void *context,uint32_t address,size_t *remaining);
bool resource_scene_dma(void *context,uint32_t source,uint16_t destination,const uint8_t *data,unsigned bytes);
#endif
