#ifndef RESOURCE_KONAMI_H
#define RESOURCE_KONAMI_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
bool resource_konami_bind(const void *data,size_t bytes);
/* IP FF2E46..FF3082, one original VBlank update; NTSC source only for now.
 * Returns1 when the original fade/hold finishes,0 while active,-1 on error. */
int resource_konami_tick(uint8_t original_vblank_counter);
bool resource_konami_draw(uint16_t output[256*192]);
const uint8_t *resource_konami_state(void); /* Native E000..EFFF workspace. */
unsigned resource_konami_music_command(void); /* Original3F request, once. */
#endif
