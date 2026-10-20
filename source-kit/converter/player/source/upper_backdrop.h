#ifndef UPPER_BACKDROP_H
#define UPPER_BACKDROP_H
#include <stdbool.h>
#include <stdint.h>
bool upper_backdrop_begin(void);
bool upper_backdrop_draw(uint16_t output[256*192]);
bool upper_backdrop_draw_rows(uint16_t *,unsigned,unsigned);
#endif
