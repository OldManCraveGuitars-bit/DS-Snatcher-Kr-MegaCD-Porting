#ifndef RESOURCE_SEGA_H
#define RESOURCE_SEGA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Original BIOS68C4 license animation. Local user-generated NSBG/NSGA input;
 * no framebuffer captures, precomputed animation or original BIOS in code. */
bool resource_sega_bind(void *data,size_t bytes);
bool resource_sega_draw(unsigned source_frame,uint16_t output[256*192]);
const uint8_t *resource_sega_actor_state(void);
#define RESOURCE_SEGA_FRAMES 363u
#endif
