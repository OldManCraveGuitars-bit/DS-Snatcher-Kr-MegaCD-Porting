#include "portrait_motion.h"

uint16_t portrait_images[3][4096] __attribute__((aligned(4)));
unsigned portrait_generation;
bool portrait_live_active;

/* Keep the existing boundary notification API for source voice/entry callers. */
void portrait_motion_end(void){portrait_generation++;}

void portrait_live_mode(bool enabled){
    if(portrait_live_active==enabled)return;
    portrait_live_active=enabled;
    (void)portrait_live_source(NULL,0);portrait_generation++;
}
