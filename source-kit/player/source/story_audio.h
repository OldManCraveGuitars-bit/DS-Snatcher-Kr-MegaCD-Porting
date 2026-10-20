#pragma once
#include <stdbool.h>
bool story_audio_begin(unsigned voice);
bool story_audio_prepare(unsigned voice);
bool story_audio_start(void);
bool story_audio_running(void);
unsigned story_audio_samples(void);
// Return1 while playing,0 when finished,-1 on storage/pose failure.
int story_audio_update(void);
void story_audio_end(void);
extern unsigned story_audio_voice;
extern bool story_audio_caption_active;
