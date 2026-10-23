#ifndef COMBAT_INDICATOR_H
#define COMBAT_INDICATOR_H
#include <stdint.h>
#include <stdbool.h>
/* The casing lamp occupies x5..6, y24..36, outside every damage crack and
 * LIFE/portrait rectangle. Cache only its original 26 theme pixels. */
static void combat_indicator_draw(uint16_t *pixels,unsigned phase,bool fresh){
    static uint16_t background[13][2];
    for(unsigned y=0;y<13;y++)for(unsigned x=0;x<2;x++){
        unsigned at=(24+y)*256+5+x;
        if(fresh)background[y][x]=pixels[at];
        else pixels[at]=background[y][x];
    }
    for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++)
        pixels[(24+phase+y)*256+5+x]=0x8000|12|(7<<5)|(2<<10);
}
#endif
