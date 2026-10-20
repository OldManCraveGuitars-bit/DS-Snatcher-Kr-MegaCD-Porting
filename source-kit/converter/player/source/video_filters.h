#pragma once
#include <stdbool.h>
#include <stdint.h>
void video_filters_init(void);
void video_filters_vblank(bool upper,bool lower_combat);
uint16_t video_filters_warm(uint16_t color);
void video_filters_copy(const uint16_t *source,uint16_t *destination,unsigned count);

void video_filters_practice(bool active);
