#ifndef NATIVE_STARTUP_H
#define NATIVE_STARTUP_H
#include <stdint.h>
#include <stdbool.h>
#define NATIVE_STARTUP_COPYRIGHT_FIRST 363u
#define NATIVE_STARTUP_COPYRIGHT_END (NATIVE_STARTUP_COPYRIGHT_FIRST+180u)
#define NATIVE_STARTUP_KONAMI_FIRST (NATIVE_STARTUP_COPYRIGHT_END+2u)
#define NATIVE_STARTUP_FRAMES (NATIVE_STARTUP_KONAMI_FIRST+789u)
bool native_startup_begin(void);
void native_startup_end(void);
bool native_startup_draw(unsigned source_frame,uint16_t output[256*192]);
/* KONAMI only; SEGA/license text retains the native RGB viewport. */
bool native_startup_draw_indexed(unsigned source_frame,uint8_t output[256*192]);
const uint16_t *native_startup_palette(void);
bool native_startup_title(uint16_t output[256*192]);
#endif
