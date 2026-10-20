#pragma once
#include "ym2612.h"
#include "resource_pcm_output.h"
typedef union {
    FM_CH fm;
    resource_pcm_output pcm;
} audio_hot_scratch;
extern audio_hot_scratch audio_hot_state;
