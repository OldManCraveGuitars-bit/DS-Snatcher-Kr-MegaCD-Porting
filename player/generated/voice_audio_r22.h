#pragma once
#include <stdint.h>
static const unsigned voice_sizes[] = {64836,82948,63776,117640,103800,87604,20160,145272,27912};
static const unsigned voice_portrait_masks[] = {1,5,7,5,5,7,5,7,5};
static const uint32_t voice_gap_ticks[] = {25428734,24666291,27033216,24565749,25814145,15500217,24678858,17297404,0};
static const unsigned voice_caption_start[] = {0,1,2,3,4,5,6,7,9};
static const unsigned voice_caption_count[] = {1,1,1,1,1,1,1,2,1};
static const uint32_t voice_caption_ticks[] = {0,0,0,0,0,0,0,0,124609842,0};
#define VOICE_RATE 16277
#define VOICE_COUNT 9
#define VOICE_TAIL_TICKS 19692257
#define VOICE_CLEAR_TICKS 12847026