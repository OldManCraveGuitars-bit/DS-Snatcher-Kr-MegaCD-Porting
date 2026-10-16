#ifndef RESOURCE_BACKGROUND_H
#define RESOURCE_BACKGROUND_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
/* Set before binding resources. The black VDP backdrop can reveal DS UI art.
 * Coloured backdrop flashes and nonzero pattern colors remain opaque,
 * including black ink. This is not a black RGB colour-key operation. */
void resource_background_ui_backdrop(bool enabled);
bool resource_background_has_ui_backdrop(void);
/* Text cards may use a uniform original tile as their blank background. */
void resource_background_ui_blank(unsigned palette_index);
bool resource_background_bind(const void *data,size_t size);
/* Live original VRAM binding; no intermediate screenshot/frame bank. */
bool resource_background_bind_vram(const uint8_t vram[65536],const uint16_t palette[64],
    unsigned plane_width,unsigned plane_height,unsigned first_plane,unsigned second_plane,
    unsigned x,unsigned y,unsigned crop_width,unsigned crop_height);
/* Native VSRAM words per original scanline; NULL disables raster overrides.
 * Visibility gates cover sprites and planes, preserving the native backdrop. */
bool resource_background_raster(const int16_t vertical_words[2][224],const uint8_t visible[224]);
/* VDP horizontal line-scroll words, sampled by original visible scanline.
 * NULL restores the common per-plane horizontal scroll registers. */
bool resource_background_line_scroll(const int16_t horizontal_words[2][224]);
bool resource_background_draw(uint16_t output[256*192]);
/* Same RGB555 pixels via cached native colour numbers. Palette-only fades
 * do not rebuild dot ownership, plane priority or decoded sprite colours. */
bool resource_background_draw_indexed_rgb555(uint16_t output[256*192]);
/* A caller-owned output lease. Zero-initialize it, and invalidate it before
 * ANY external write/reuse of that output. No rendered image is owned here.
 * Changing the destination or rebinding resources also forces a full copy. */
typedef struct {
    const uint16_t *destination;
    uint64_t revision;
} resource_background_picture;
void resource_background_picture_invalidate(resource_background_picture *picture);
bool resource_background_draw_indexed_rgb555_cached(uint16_t output[256*192],
    resource_background_picture *picture,bool *copied);
/* Direct8-bit bitmap: bit6 may be set for sprites; mirror palette0..63
 * into64..127 on DS. No frame capture, RGB expansion or quantization. */
bool resource_background_draw_indexed8(uint8_t output[256*192],unsigned first_x,unsigned last_x);
/* Full cached page with original indices shifted to128..191: black opaque. */
bool resource_background_draw_indexed8_opaque(uint8_t output[256*192]);
bool resource_background_scroll(unsigned plane,int horizontal,int vertical);
bool resource_background_palette(unsigned first,const uint16_t *words,unsigned count);
/* Call after a native command mutates pattern/plane words in the bound bank. */
void resource_background_invalidate(void);
/* Publish original VRAM into the currently bound live bank. Preserve caches
 * whose pattern and map words did not change; both arrays are full native VRAM. */
bool resource_background_update_vram(uint8_t destination[65536],const uint8_t source[65536]);
bool resource_background_update_vram_tiles(uint8_t destination[65536],const uint8_t source[65536],const uint32_t written_tiles[64]);
/* Publication may run ahead of the displayed VDP. Deferred copies must not
 * disturb the renderer bound to that separate display bank. changed receives
 * the actual changed tiles, not merely the source DMA's written ranges. */
bool resource_background_publish_vram(uint8_t destination[65536],const uint8_t source[65536],
    const uint32_t written_tiles[64],uint32_t changed[64],bool deferred);
typedef struct {
    uint32_t tile;
    uint8_t pixels[32];
} resource_background_patch;
bool resource_background_patch_vram(uint8_t destination[65536],
    const resource_background_patch *patches,unsigned count);
bool resource_background_map_changed(unsigned plane,unsigned x,unsigned y,unsigned w,unsigned h);
typedef struct {
    int16_t x,y;
    uint16_t size,attribute;
} resource_background_sprite;
/* NULL restores the immutable source-bank sprites. An empty list clears them. */
bool resource_background_sprites(const resource_background_sprite *sprites,unsigned count);
/* Original VDP plane-table register changes retain current scroll/SAT state. */
bool resource_background_planes(unsigned plane_a,unsigned plane_b);
/* Change only the viewport. Letter cards may retain native 1:1 rows. */
bool resource_background_view(unsigned x,unsigned y,unsigned width,unsigned height);
/* Presentation-only DS RGB555 palette, preserving original palette state. */
bool resource_background_palette_rgb555(const uint16_t words[64]);
/* Valid until the next indexed draw. Palette changes are independent. */
bool resource_background_last_indices_changed(void);
bool resource_background_last_row_changed(unsigned y);
bool resource_background_draw_area8_retained(uint8_t output[256*192],resource_background_picture *);
bool resource_background_draw_indexed8_area_cached(uint8_t output[256*192],resource_background_picture *,bool *,unsigned first,unsigned last,unsigned rows);
#endif

bool resource_background_draw_native_rows_retained(uint8_t output[256*192],unsigned rows,resource_background_picture *picture);

bool resource_background_draw_native_packed(uint8_t *,unsigned);

bool resource_background_draw_native_packed_retained(uint8_t *,unsigned,const uint8_t *,resource_background_picture *);
