#ifndef RESOURCE_VDP_HISTORY_H
#define RESOURCE_VDP_HISTORY_H
#include "resource_opening.h"
enum { RESOURCE_VDP_META_MAX=16+512+224*10 };
/* Runtime original VDP commands only. Never an image/video asset format. */
typedef struct {
    uint32_t bytes;
    uint16_t tiles,metadata;
    resource_background_patch patches[];
} resource_vdp_packet;
typedef struct {
    uint64_t publication;
    unsigned bytes;
    uint8_t metadata[RESOURCE_VDP_META_MAX];
    /* Main generation can be interrupted by native picture composition.
     * Keep the bounded encoder workspace off the shared6KiB user stack. */
    uint8_t scratch[RESOURCE_VDP_META_MAX];
} resource_vdp_encoder;
typedef struct {
    uint8_t vram[65536] __attribute__((aligned(4)));
    resource_background_sprite sprites[64];
    int16_t vertical[2][224],horizontal[2][224];
    uint8_t visible[224];
    uint16_t plane_b,scroll_x[2],scroll_y[2];
    unsigned sprite_count;
    bool line_scroll;
    /* Dependencies of the last displayed packed viewport. All original VRAM
     * updates still apply, including offscreen data needed by later views. */
    uint32_t visible_tiles[64];
    uint32_t visible_plane_tiles[64],visible_map_tiles[64];
    uint32_t visible_map_cells[2][64];
    unsigned visible_y,visible_rows;
    bool visible_ready,visible_dirty,visible_plane_dirty,visible_maps_dirty;
} resource_vdp_view;
/* Call at an original publication boundary before advancing the producer.
 * Initial bank is copied once; all subsequent publications must be captured. */
bool resource_vdp_history_begin(resource_vdp_encoder *,resource_vdp_view *,const resource_opening_state *);
/* NULL packet is a successful unchanged native frame. Caller owns/free()s
 * each non-NULL packet. A failed allocation retains the publication baseline. */
bool resource_vdp_history_capture(resource_vdp_encoder *,const resource_opening_state *,resource_vdp_packet **);
bool resource_vdp_history_bind(resource_vdp_view *);
bool resource_vdp_history_apply(resource_vdp_view *,const resource_vdp_packet *);
bool resource_vdp_history_changed(const resource_vdp_view *,unsigned source_y,unsigned rows);
bool resource_vdp_history_draw(resource_vdp_view *,uint8_t *,unsigned source_y,unsigned rows,
    const uint8_t *previous,resource_background_picture *);
#endif
