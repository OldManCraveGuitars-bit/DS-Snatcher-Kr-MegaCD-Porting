#pragma once
#include "portrait_live.h"
extern bool portrait_live_active;
void portrait_live_mode(bool enabled);
#include <stdint.h>
#include <stdbool.h>
extern unsigned portrait_generation;
void portrait_motion_end(void);

