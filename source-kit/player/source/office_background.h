#ifndef OFFICE_BACKGROUND_H
#define OFFICE_BACKGROUND_H
#include <stdbool.h>
#include <stdint.h>
bool office_background_draw(uint16_t output[256*192]);
void office_background_end(void);
#endif
