#ifndef RESOURCE_FLASHLIGHT_H
#define RESOURCE_FLASHLIGHT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "resource_scroll.h"
/* Original SUB96D0..96DD, including fixed point position and target96DC. */
enum { RESOURCE_FLASHLIGHT_BYTES=14 };
bool resource_flashlight_input(uint8_t state[14],const uint8_t *sub,size_t bytes,
    unsigned held,unsigned history,uint8_t *menu_lock,uint8_t *cursor);
void resource_flashlight_present(const uint8_t state[14],unsigned mode,
    uint8_t globals[128],resource_scroll_state *scroll);
/* Touch changes the same original coordinates, never the target/clue flags. */
bool resource_flashlight_touch(uint8_t state[14],int x,int y);
bool resource_flashlight_valid(const uint8_t state[14]);
#endif
