#ifndef RESOURCE_SCROLL_H
#define RESOURCE_SCROLL_H
#include <stdint.h>
#include <stdbool.h>
/* Native SUBCODE F3F8/F446 command bridge, prior to Main IP visual modes. */
typedef struct {
    uint8_t control[64]; /* 8F00 */
    uint8_t shared[24];  /* B6408 */
    uint8_t output[8];   /* FF8026..FF802D */
    uint8_t changed;     /* 78C7 */
} resource_scroll_state;
void resource_scroll_mode(resource_scroll_state *state,unsigned mode);
bool resource_scroll_actor(resource_scroll_state *state,unsigned opcode,unsigned arg,uint16_t value);
void resource_scroll_tick(resource_scroll_state *state,uint32_t clear_fill);
#endif
