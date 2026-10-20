#ifndef RESOURCE_GRAPHICS_H
#define RESOURCE_GRAPHICS_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
/* SUBCODE E81A/E846: seven eight-byte jobs and four original scratch banks.
 * Source spans contain user-supplied resource/SUBCODE bytes. DMA publishes
 * the current scratch bytes; it never uses a captured video frame. */
typedef struct {
    uint8_t jobs[7*8];
    uint8_t scratch[4*8192];
    uint32_t bank;
    uint8_t error;
} resource_graphics_state;
typedef const uint8_t *(*resource_graphics_source)(void *context,uint32_t address,size_t *remaining);
typedef bool (*resource_graphics_dma)(void *context,uint32_t source,uint16_t destination,const uint8_t *data,unsigned bytes);
void resource_graphics_clear(resource_graphics_state *state);
bool resource_graphics_enqueue(resource_graphics_state *state,resource_graphics_source source,void *context,uint32_t command);
bool resource_graphics_tick(resource_graphics_state *state,resource_graphics_source source,
    resource_graphics_dma dma,void *context,bool freeze,bool dma_busy,uint16_t zero_word);
#endif
