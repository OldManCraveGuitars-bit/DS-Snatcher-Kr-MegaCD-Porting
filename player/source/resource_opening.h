#ifndef RESOURCE_OPENING_H
#define RESOURCE_OPENING_H
#include "resource_scene.h"
#include "resource_callback.h"
#include "resource_cards.h"
#include "resource_sprites.h"
#include "resource_background.h"
typedef struct {
    resource_scene_state scene;
    resource_callback_state callbacks;
    resource_cards_state cards;
    uint8_t front_vram[65536] __attribute__((aligned(4)));
    uint8_t pending_sat[512],front_sat[512];
    uint16_t front_palette[64];
    resource_background_sprite sprites[64];
    int16_t raster_vertical[2][224];
    uint8_t raster_visible[224];
    uint32_t frames,published_transfers,clear_count,published_clears;
    uint32_t vdp_command;
    uint16_t live_vertical[2];
    unsigned pending_sprites,front_sprites,scene_id;
    uint8_t visible,finished,error,raster_active;
    uint8_t plane_request[2],hardware_flags;
    uint16_t plane_b,cd_request,cd_status;
    uint16_t pending_palette[64];
    bool delayed_palette;
    uint8_t main_horizontal[896]; /* Main E700 line-scroll work table. */
    int16_t raster_horizontal[2][224]; /* Published VDP line-scroll words. */
    /* Port-only provenance of the exact pending/published native SAT.
     * Not a face schedule; it follows the same one-tick publication. */
    uint8_t pending_owners[64],front_owners[64];
    uint32_t pending_maps[64],front_maps[64];
    uint16_t pending_portrait_palette[64],front_portrait_palette[64];
    bool pending_provenance_valid,front_provenance_valid;
    /* Runtime cache evidence only; all original VRAM writes still publish. */
    uint32_t portrait_written_tiles[64];
    uint64_t portrait_publication;
    /* Optional native VDP playback queue; ordinary live rendering is unchanged. */
    uint32_t display_changed_tiles[64];
    bool deferred_display;

} resource_opening_state;
bool resource_opening_bind(resource_opening_state *state,const void *bank,size_t bytes,
    const void *subcode,size_t subcode_bytes,uint32_t rng);
/* Shared native Main/SUB publication pipeline for script-driven scenes. */
bool resource_opening_bind_scene(resource_opening_state *state,const void *resource,size_t bytes,
    const void *subcode,size_t subcode_bytes,uint32_t rng);
bool resource_opening_publish(resource_opening_state *state);
bool resource_opening_finish_tick(resource_opening_state *state);
bool resource_opening_tick(resource_opening_state *state,uint32_t cd_clock_bcd,uint16_t iterations);
/* The real source/stream device supplies status100 on CD completion.
 * A command is exposed in cd_request; the caller consumes it explicitly. */
bool resource_opening_tick_cd(resource_opening_state *state,uint32_t cd_clock_bcd,uint16_t iterations,uint16_t cd_status);
bool resource_opening_draw(resource_opening_state *state,uint16_t output[256*192]);
bool resource_opening_draw_view(resource_opening_state *state,uint16_t output[256*192],
    const uint16_t rgb555[64],unsigned source_y,unsigned source_height);
bool resource_opening_draw_view_indexed_rgb555_cached(resource_opening_state *state,uint16_t output[256*192],
    const uint16_t rgb[64],unsigned y,unsigned height,resource_background_picture *picture,bool *copied);
bool resource_opening_draw_view_indexed_rgb555(resource_opening_state *state,uint16_t output[256*192],
    const uint16_t rgb555[64],unsigned source_y,unsigned source_height);
bool resource_opening_draw_area(resource_opening_state *state,uint16_t output[256*192],
    const uint16_t rgb555[64],unsigned x,unsigned y,unsigned width,unsigned height);
/* Native8-bit entry viewport; only bitmap columns32..223 are published. */
bool resource_opening_draw_area8(resource_opening_state *state,uint8_t output[256*192],unsigned x,unsigned y,unsigned width,unsigned height);
bool resource_opening_draw_view8(resource_opening_state *state,uint8_t output[256*192],unsigned source_y,unsigned source_height);
bool resource_opening_draw_area8_retained(resource_opening_state *,uint8_t out[256*192],unsigned,unsigned,unsigned,unsigned,resource_background_picture *);
bool resource_opening_draw_room_indices_cached(resource_opening_state *,uint8_t out[256*192],const uint16_t rgb[64],resource_background_picture *,bool *);
#endif

bool resource_opening_draw_full_indices_cached(resource_opening_state *,uint8_t [256*192],const uint16_t [64],resource_background_picture *,bool *);

bool resource_opening_draw_native_rows_retained(resource_opening_state *s,uint8_t out[256*192],unsigned y,unsigned rows,resource_background_picture *picture);

bool resource_opening_draw_native_packed(resource_opening_state *,uint8_t *,unsigned,unsigned);

bool resource_opening_draw_native_packed_retained(resource_opening_state *,uint8_t *,unsigned,unsigned,const uint8_t *,resource_background_picture *);

bool resource_opening_draw_practice_indices(resource_opening_state *,uint8_t [256*192],
    const uint16_t [64],resource_background_picture *,bool *);

bool resource_opening_draw_terminal_indices(resource_opening_state *,uint8_t [256*192],const uint16_t [64],resource_background_picture *,bool *);

/* Complete fitted raster using in-place indexed expansion; no RGB cache. */
bool resource_opening_draw_area_compact(resource_opening_state *,uint16_t [256*192],const uint16_t [64],unsigned,unsigned,unsigned,unsigned);
