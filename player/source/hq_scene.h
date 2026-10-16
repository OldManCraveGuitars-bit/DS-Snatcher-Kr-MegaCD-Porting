#pragma once
#include <stdint.h>
#include <stdbool.h>
// Append IDs to the released0..4 scene namespace; source IDs remain unchanged.
#define HQ_SCENE(code) (0x4000u|(code))
#define HQ_OVERLAY(code) (0x8000u|(code))
bool hq_scene_valid(unsigned scene);
bool hq_scene_read(unsigned scene,uint16_t *pixels);
unsigned hq_scene_under(unsigned scene);
