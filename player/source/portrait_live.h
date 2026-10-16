#pragma once
#include "resource_portrait_history.h"
extern unsigned portrait_generation;
bool portrait_live_source(const resource_opening_state *video,unsigned epoch);
bool portrait_live_snapshot(const resource_portrait_snapshot *snapshot);
unsigned portrait_live_count(void);
int portrait_live_x(unsigned slot);
const uint16_t *portrait_live_image(unsigned slot);
/* Ending speech does not delete original actors during its following wait. */

