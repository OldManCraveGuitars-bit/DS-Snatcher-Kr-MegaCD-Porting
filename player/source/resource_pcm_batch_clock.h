#pragma once
#include <stdint.h>

/* Exact ceil((count*2^52-next)/factor), bounded to the64-frame chip batch.
 * At32000Hz each output needs one chip clock, with at most two extras.
 * The retained fractional phase decides those extras without division. */
static inline unsigned resource_pcm_batch_clocks(uint64_t factor,uint64_t next,unsigned count){
    const uint64_t unit=UINT64_C(1)<<52;
    if(factor==UINT64_C(4427218577690293) && count<=64 && next<factor){
        int64_t remaining=(int64_t)next-(int64_t)(unit-factor)*count;
        unsigned clocks=count;
        while(remaining<0){remaining+=(int64_t)factor;clocks++;}
        return clocks;
    }
    uint64_t duration=unit*count;
    return duration>next?(unsigned)((duration-next+factor-1)/factor):0;
}
