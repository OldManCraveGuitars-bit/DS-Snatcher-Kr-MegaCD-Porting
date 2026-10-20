#include "title_badge.h"
#include <stdbool.h>
#define GOLD(r,g,b) (0x8000u|((r)&31)|(((g)&31)<<5)|(((b)&31)<<10))
/* Hand-authored lettering, expanded at integer dot sizes. Keep this addition
 * separate from the untouched original DATA_D2 title tiles and palette. */
static const uint8_t letters[2][9]={
    {0x7c,0x7e,0x63,0x63,0x63,0x63,0x63,0x7e,0x7c},
    {0x3e,0x7f,0x60,0x60,0x3e,0x03,0x03,0x7f,0x7e}
};
static bool ink(unsigned letter,int x,int y){
    return x>=0 && x<14 && y>=0 && y<18 && (letters[letter][y/2]&(1u<<(6-x/2)));
}
void title_badge_draw(uint16_t output[256*192]){
    if(!output)return;
    const unsigned left=194,top=93;
    /* Match the original logo's lit face and short down-right gold edge.
     * Shade continuously across all18 rows; avoid dark rectangular patches. */
    for(unsigned depth=2;depth;depth--)for(unsigned l=0;l<2;l++)for(int y=0;y<18;y++)for(int x=0;x<14;x++){
        if(!ink(l,x,y))continue;
        unsigned sx=left+l*17+x+(17-y)/4+depth,sy=top+y+depth;
        output[sy*256+sx]=GOLD(25-y*4/17,18-y*5/17,4-y*2/17);
    }
    /* Hard native-dot boundaries keep the pale gold face distinct from the
     * noisy gold backdrop. No blur, antialiasing or fractional glyph scaling. */
    for(unsigned l=0;l<2;l++)for(int y=0;y<18;y++)for(int x=0;x<14;x++){
        if(!ink(l,x,y))continue;
        unsigned sx=left+l*17+x+(17-y)/4,sy=top+y;
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
            output[(sy+dy)*256+sx+dx]=GOLD(19,13,2);
    }
    for(unsigned l=0;l<2;l++)for(int y=0;y<18;y++)for(int x=0;x<14;x++){
        if(!ink(l,x,y))continue;
        unsigned sx=left+l*17+x+(17-y)/4,sy=top+y;
        unsigned r=31-y/17,g=30-y*7/17,b=20-y*14/17;
        uint16_t color=GOLD(r,g,b);
        if(!ink(l,x,y-1) || !ink(l,x-1,y))color=GOLD(31,31-y*3/17,25-y*9/17);
        else if(!ink(l,x,y+1) || !ink(l,x+1,y))color=GOLD(27,20,5);
        output[sy*256+sx]=color;
    }
}
