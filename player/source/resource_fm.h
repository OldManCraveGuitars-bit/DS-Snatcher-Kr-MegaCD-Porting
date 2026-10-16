#pragma once
#include <stdint.h>
#include <stdbool.h>
bool resource_fm_open(unsigned command);
void resource_fm_close(void);
bool resource_fm_active(void);
bool resource_fm_render(int16_t *out,unsigned frames);
