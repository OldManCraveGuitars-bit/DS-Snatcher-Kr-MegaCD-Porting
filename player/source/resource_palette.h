#ifndef RESOURCE_PALETTE_H
#define RESOURCE_PALETTE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* Original SUBCODE E41A/E4B4:12 jobs,128 native Mega CD colour words. */
typedef struct {
    uint8_t jobs[12*16];
    uint16_t colors[128];
    uint8_t cursor,changed,error;
} resource_palette_state;
void resource_palette_reset(resource_palette_state *state);
typedef const uint8_t *(*resource_palette_source)(void *context,uint32_t address,size_t *remaining);
bool resource_palette_enqueue_resolved(resource_palette_state *state,resource_palette_source source,void *context,uint32_t command);
bool resource_palette_tick_resolved(resource_palette_state *state,resource_palette_source source,void *context,bool freeze);
bool resource_palette_enqueue(resource_palette_state *state,const void *data,size_t size,uint32_t base,uint32_t command);
bool resource_palette_tick(resource_palette_state *state,const void *data,size_t size,uint32_t base,bool freeze);
#endif
