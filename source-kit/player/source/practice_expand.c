#include "practice_expand.h"
#include <string.h>
/* Exactly floor(x*224/256): each seven source dots occupy eight output
 * dots, in the repeated sequence0,0,1,2,3,4,5,6. Palette zero is opaque.
 * memcpy stores avoid alias/alignment assumptions about the caller's u16
 * array; ARM optimizes the fixed-size stores to ordinary words. */
#ifdef __NDS__
extern void practice_expand_row_arm(const uint8_t *,const uint16_t *,uint16_t *) __attribute__((long_call));
#endif
static void expand_row(const uint8_t *src,const uint16_t *palette,uint16_t *dst){
#ifdef __NDS__
    if(!(((uintptr_t)src|(uintptr_t)dst)&3u)){
        practice_expand_row_arm(src,palette,dst);return;
    }
#endif
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

/* Exact source rows, not a lossy hash. The combat caller owns the retained
 * output and forces a refresh after slides, buffer changes or UI takeover.
 * Source palette changes invalidate all rows; original opaque ink0 stays. */
static uint8_t practice_row_source[128][224];
static uint16_t practice_row_palette[64];
static bool practice_row_valid;
unsigned char practice_dirty_rows[192];
volatile unsigned practice_rows_expanded,practice_rows_transferred;
bool practice_expand_rgb555_changed_rows(const uint8_t *indices,const uint16_t *palette,uint16_t *output,bool force,const bool *rows){
    force=force || !practice_row_valid || memcmp(palette,practice_row_palette,sizeof(practice_row_palette));
    memset(practice_dirty_rows,0,sizeof(practice_dirty_rows));
    bool any=false;
    for(unsigned y=0;y<128;y++){
        const uint8_t *row=indices+y*256+16;
        if(!force && rows && !rows[y])continue;
        if(!force && !memcmp(row,practice_row_source[y],224))continue;
        memcpy(practice_row_source[y],row,224);
        unsigned first=(y*3+1)/2,last=((y+1)*3+1)/2;
        expand_row(row,palette,output+first*256);
        practice_dirty_rows[first]=1;practice_rows_expanded++;
        if(last-first==2){memcpy(output+(first+1)*256,output+first*256,512);practice_dirty_rows[first+1]=1;}
        any=true;
    }
    memcpy(practice_row_palette,palette,sizeof(practice_row_palette));practice_row_valid=true;
    return any;
}

bool practice_expand_rgb555_changed(const uint8_t *indices,const uint16_t *palette,uint16_t *output,bool force){
    return practice_expand_rgb555_changed_rows(indices,palette,output,force,NULL);
}
