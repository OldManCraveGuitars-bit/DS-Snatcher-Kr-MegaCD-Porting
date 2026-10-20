#ifndef RESOURCE_MAIN_SCROLL_H
#define RESOURCE_MAIN_SCROLL_H
#include "resource_cards.h"

/* Main IP FF34DA..FF37FE: original secondary horizontal-scroll modes3..6.
 * The896-byte table corresponds to E700..EA7F, two words per scanline.
 * Main dispatch owns mode requests; frame is its original E020 counter. */
bool resource_main_scroll_tick(resource_cards_state *state,
    uint8_t horizontal_lines[896],uint16_t frame,
    resource_cards_port port,void *context);
#endif
