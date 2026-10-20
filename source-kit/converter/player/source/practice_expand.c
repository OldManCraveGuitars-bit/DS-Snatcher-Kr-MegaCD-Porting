#include "practice_expand.h"
#include <string.h>
/* Exactly floor(x*224/256): each seven source dots occupy eight output
 * dots, in the repeated sequence0,0,1,2,3,4,5,6. Palette zero is opaque.
 * memcpy stores avoid alias/alignment assumptions about the caller's u16
 * array; ARM optimizes the fixed-size stores to ordinary words. */
static void expand_row(const uint8_t *src,const uint16_t *palette,uint16_t *dst){
    for(unsigned group=0;group<32;group++,src+=7,dst+=8){
        uint32_t c0=palette[src[0]&63];
        uint32_t words[4]={c0|(c0<<16),
            palette[src[1]&63]|((uint32_t)palette[src[2]&63]<<16),
            palette[src[3]&63]|((uint32_t)palette[src[4]&63]<<16),
            palette[src[5]&63]|((uint32_t)palette[src[6]&63]<<16)};
        for(unsigned i=0;i<4;i++)words[i]|=0x80008000u;
        memcpy(dst,words,sizeof(words));
    }
}
void practice_expand_rgb555(const uint8_t *indices,const uint16_t *palette,uint16_t *output){
    /* Exactly floor(y*128/192): source rows0,1 become display0,0,1. */
    for(unsigned pair=0;pair<64;pair++,indices+=512,output+=768){
        expand_row(indices+16,palette,output);
        memcpy(output+256,output,512);
        expand_row(indices+256+16,palette,output+512);
    }
}
