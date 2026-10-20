#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Runtime player state only: source fields/RNG at an actual ACT entrance.
 * No pointers, media, emulator state or rendered images are stored here. */
#define CHAPTER_SEED_BYTES 360u
void chapter_progress_load(void);
bool chapter_progress_cleared(void);
bool chapter_progress_available(unsigned chapter);
bool chapter_progress_record(unsigned chapter,const uint8_t seed[CHAPTER_SEED_BYTES]);
bool chapter_progress_clear(void);
bool chapter_progress_read(unsigned chapter,uint8_t seed[CHAPTER_SEED_BYTES]);
