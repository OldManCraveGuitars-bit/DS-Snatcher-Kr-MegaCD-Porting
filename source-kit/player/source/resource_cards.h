#ifndef RESOURCE_CARDS_H
#define RESOURCE_CARDS_H
#include <stdint.h>
#include <stdbool.h>
/* Main IP FF3836 and HBlank FF0DB0..FF0EA6, original opening modes8..12.
 * These operate on the original Main fields and emit typed VDP port writes.
 * The caller is responsible for original VBlank/HBlank ordering. */
typedef struct {
    uint8_t main[144]; /* E000..E08F */
    uint8_t cached_registers[16]; /* FFEF00..FFEF0F */
    uint8_t vscroll[4],hscroll[4]; /* E700,EB00 */
} resource_cards_state;
typedef bool (*resource_cards_port)(void *context,uint32_t address,unsigned size,uint32_t value);
bool resource_cards_tick(resource_cards_state *state,resource_cards_port port,void *context);
bool resource_cards_hblank(resource_cards_state *state,unsigned counter,resource_cards_port port,void *context);
/* Original Main FF332C primary request handling, including telephone mode23
 * and its deferred mode15 restoration. A zero request keeps the active mode. */
bool resource_cards_primary(resource_cards_state *state,unsigned request,resource_cards_port port,void *context);
#endif
