#ifndef RESOURCE_AUDIO_CLOCK_H
#define RESOURCE_AUDIO_CLOCK_H
#include <stdint.h>
/* Exact original clocks. No quantization or approximate reciprocal.
 * 0 <= phase < 32,256,000; 0 <= block <= 64, hence the numerator
 * is at most 3,468,619,199 and fits an unsigned 32-bit ARM multiply. */
static inline unsigned resource_fm_native_count(unsigned phase,unsigned block){
    return (phase+block*53693175u)/32256000u;
}
/* The event loop caps delta by its remaining <=128 native samples. */
static inline unsigned resource_fm_native_distance(uint64_t delta){
    return ((uint32_t)delta+1007u)/1008u;
}
static inline uint64_t resource_audio_frame_boundary(unsigned frame){
    /* Cancel the exact common factor75 before multiplying. The maximum
     * numerator for a 32-bit frame, including ceil bias, fits uint64_t. */
    return ((uint64_t)frame*382310400u+715908u)/715909u;
}
#endif
