#ifndef RESOURCE_AUDIO_CLOCK_H
#define RESOURCE_AUDIO_CLOCK_H
#include <stdint.h>
/* Exact original clocks, 0 <= block <= 128. Split the numerator
 * into <=64-output-sample pieces so every ARM multiply stays within 32 bits.
 * At step4032 the phase is less than129,024,000, and a64-sample numerator
 * is at most3,565,387,199. No oscillator or source tick is omitted. */
static inline unsigned resource_fm_native_count(unsigned phase,unsigned block){
    unsigned count=0;
    if(block>64){
        unsigned advance=phase+64u*53693175u;
        count=advance/129024000u;phase=advance-count*129024000u;block-=64;
    }
    return count+(phase+block*53693175u)/129024000u;
}
/* The event loop caps delta by its remaining <=128 native samples. */
static inline unsigned resource_fm_native_distance(uint64_t delta){
    return ((uint32_t)delta+4031u)/4032u;
}
static inline uint64_t resource_audio_frame_boundary(unsigned frame){
    /* Cancel the exact common factor75 before multiplying. The maximum
     * numerator for a 32-bit frame, including ceil bias, fits uint64_t. */
    return ((uint64_t)frame*382310400u+715908u)/715909u;
}
#endif
