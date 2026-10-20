#pragma once
#include <stdbool.h>
#include <stdint.h>
bool native_credits_begin(uint32_t rng,uint8_t special);
void native_credits_end(void);
bool native_credits_fill(int16_t *out,unsigned frames);
/* The caller supplies the continuously played 32kHz DAC position. */
int native_credits_draw(unsigned played,uint8_t out[256*192]);
void native_credits_input(bool pressed);
bool native_credits_waiting(void);
unsigned native_credits_track(void);
unsigned native_credits_position(unsigned played);
const uint16_t *native_credits_palette(void);
