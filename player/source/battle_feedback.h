#pragma once
#include <stdint.h>
/* Port-only presentation requested by the user; never consumes game RNG. */
unsigned battle_feedback_face(unsigned damage);
unsigned battle_feedback_flash(unsigned elapsed);
void battle_feedback_draw(uint16_t pixels[256*192],const uint16_t faces[4*4096],unsigned damage);
