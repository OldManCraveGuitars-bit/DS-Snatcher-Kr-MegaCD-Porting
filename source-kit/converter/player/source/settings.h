#pragma once
#include <stdbool.h>
extern bool settings_buttons;
extern bool settings_scanlines;
extern bool settings_tv_filter;
extern unsigned settings_level;
void settings_load(void);
bool settings_store(void);
unsigned settings_volume(void);
