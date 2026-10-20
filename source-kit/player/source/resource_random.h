#ifndef RESOURCE_RANDOM_H
#define RESOURCE_RANDOM_H
#include <stdint.h>
/* Original SUB FD96; FD8C scales the returned high word with MULU/SWAP. */
static inline uint16_t resource_random_next(uint32_t *state){
    unsigned left=*state>>16,right=*state&65535;
    left=(left&255)|((((left>>8)+(left&255))&255)<<8);
    left=(left+right+((right<<8)|(right>>8)))&65535;
    unsigned sum=(right>>8)+(right&255);
    right=(right&255)|((sum&255)<<8);
    if(sum>255)left=(left+1)&65535;
    right++;
    if(right>65535)left=(left+1)&65535;
    *state=(uint32_t)left<<16|(right&65535);
    return (uint16_t)left;
}
#endif
