#ifndef NATIVE_OPENING_H
#define NATIVE_OPENING_H
#include <stdbool.h>
#include <stdint.h>
unsigned native_opening_picture_height(void);
bool native_opening_begin(uint32_t rng);
bool native_opening_begin_track(unsigned track,uint32_t rng);
/* D0 device bridge: one original tick per call, source CD clock in75Hz units. */
bool native_opening_tick_cd(unsigned cd_frame,uint16_t status);
bool native_opening_draw_current(uint8_t output[256*192]);
const uint16_t *native_opening_palette(void);
unsigned native_opening_take_cd_request(void);
bool native_opening_finished(void);
void native_opening_end(void);
bool native_opening_draw(unsigned source_frame,uint8_t output[256*192]);
#endif
