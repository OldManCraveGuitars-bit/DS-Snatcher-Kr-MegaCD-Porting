#ifndef RESOURCE_VOICE_GAIN_H
#define RESOURCE_VOICE_GAIN_H
#include <stdint.h>
/* SUB configures four identical speech channels, swaps the descriptor pan
 * nibbles, floors each channel, then the PCM chip saturates their sum. This
 * is amplitude/pan adaptation only; the existing DS speech clock remains. */
static inline int16_t resource_voice_gain_sample(int sample,unsigned envelope,unsigned pan){
#if (-1 >> 1) != -1
#error Speech gain requires arithmetic signed shifts
#endif
    int value=4*((sample*(int)(envelope*pan))>>5);
    return value>32767?32767:value< -32768?-32768:value;
}
#endif
