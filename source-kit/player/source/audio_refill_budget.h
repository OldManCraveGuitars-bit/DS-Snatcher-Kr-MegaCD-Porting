#ifndef AUDIO_REFILL_BUDGET_H
#define AUDIO_REFILL_BUDGET_H
/* Reserve determines a bounded foreground quantum, never source timing.
 * Every completed callback is published before producing the next chunk. */
static inline unsigned audio_refill_quantum(int pending, unsigned primed){
    return primed && pending>(int)(primed/4)?512u:256u;
}
static inline unsigned audio_refill_take(unsigned requested,unsigned *remaining,
                                       int sliced,int chunked){
    if(sliced && requested>*remaining)requested=*remaining;
    if(chunked && requested>256)requested=256;
    if(sliced)*remaining-=requested;
    return requested;
}
#endif
