#include <string.h>
#include "battle_slide.h"
unsigned battle_slide_offset(unsigned tick,bool returning){
    unsigned d=BATTLE_SLIDE_FRAMES;
    if(tick>d)tick=d;
    unsigned y=(192u*tick*tick*(3u*d-2u*tick)+d*d*d/2)/(d*d*d);
    return returning?192u-y:y;
}
void battle_slide_blit(const uint16_t *scene,uint16_t *upper,uint16_t *lower,unsigned tick,bool returning){
    unsigned offset=battle_slide_offset(tick,returning);
    for(unsigned y=0;y<192;y++){
        unsigned virtual_y=offset+y;
        uint16_t *destination=virtual_y<192?upper+virtual_y*256:lower+(virtual_y-192)*256;
        memcpy(destination,scene+y*256,256*sizeof(uint16_t));
    }
}

unsigned battle_slide_dma_ranges(unsigned previous,unsigned offset,battle_slide_copy spans[3]){
    unsigned n=0;
    if(previous>192)previous=192;
    if(offset>192)offset=192;
    if(offset>previous)spans[n++]=(battle_slide_copy){1,0,previous,previous,offset-previous};
    else if(offset<previous)spans[n++]=(battle_slide_copy){2,1,offset,offset,previous-offset};
    if(offset<192)spans[n++]=(battle_slide_copy){0,0,0,offset,192-offset};
    if(offset)spans[n++]=(battle_slide_copy){0,1,192-offset,0,offset};
    return n;
}
