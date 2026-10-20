#pragma once
#include <stdint.h>
#include <stdbool.h>
#define BATTLE_SLIDE_FRAMES 36u
unsigned battle_slide_offset(unsigned tick,bool returning);
void battle_slide_blit(const uint16_t *scene,uint16_t *upper,uint16_t *lower,unsigned tick,bool returning);

/* Up to three opaque spans: restored casing, upper scene, lower scene.
 * source:0=immutable scene,1=upper staging,2=lower staging. */
typedef struct {unsigned source,screen,source_row,destination_row,rows;} battle_slide_copy;
unsigned battle_slide_dma_ranges(unsigned previous,unsigned offset,battle_slide_copy spans[3]);
