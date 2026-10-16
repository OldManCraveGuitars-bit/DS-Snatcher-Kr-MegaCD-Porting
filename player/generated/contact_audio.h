#pragma once
#include <stdint.h>
static const uint16_t contact_voice_ids[] = {1185,1186,1187,1188,1189,1190,1191,1193};
static const uint32_t contact_voice_sizes[] = {33384,41644,112216,15268,73008,56244,108908,187408};
static const uint16_t contact_caption_starts[] = {0,1,2,4,5,6,7,9};
static const uint8_t contact_caption_counts[] = {1,1,2,1,1,1,2,3};
static const uint32_t contact_caption_samples[] = {0,0,0,73518,0,0,0,0,78130,0,72704,124791};
static const uint32_t contact_caption_ends[] = {0,0,0,0,0,0,0,0};
static inline int contact_voice_index(unsigned voice){for(unsigned i=0;i<8;i++)if(contact_voice_ids[i]==voice)return (int)i;return -1;}
