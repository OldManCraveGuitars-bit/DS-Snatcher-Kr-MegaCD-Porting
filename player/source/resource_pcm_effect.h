#ifndef RESOURCE_PCM_EFFECT_H
#define RESOURCE_PCM_EFFECT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Views into original PCMLT/PCMDRMDT and the selected dynamic load. No voice
 * ID aliasing: the 16-byte effect table lives at PCMLT offset4000. */
typedef struct {
    const uint8_t *table,*resident,*dynamic;
    size_t table_bytes,resident_bytes,dynamic_bytes;
} resource_pcm_effect_bank;
typedef struct {
    uint8_t priority,pair,volume,pan;
    uint16_t step,nominal_rate;
    uint32_t address,loop,encoded_bytes,samples;
    const uint8_t *encoded;
} resource_pcm_effect;
bool resource_pcm_effect_lookup(const resource_pcm_effect_bank *bank,unsigned command,resource_pcm_effect *effect);
#endif
