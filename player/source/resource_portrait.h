#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Original 4bpp patterns, sprite maps and raw Mega CD palette words.
// The container has no rendered portrait images or text layer.
bool resource_portrait_bind(const void *data, size_t bytes);
bool resource_portrait_draw(unsigned frame, uint16_t pixels[4096]);
unsigned resource_portrait_count(void);

bool resource_portrait_draw_data(const void *,size_t,unsigned,uint16_t [4096]);
