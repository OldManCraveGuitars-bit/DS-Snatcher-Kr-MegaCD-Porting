#ifndef UI_THEME_H
#define UI_THEME_H
#include <stdint.h>
#include <stdbool.h>
/* Copy an original contiguous span; destinations may have halfword alignment. */
void ui_theme_copy(uint16_t *out,bool panel,unsigned first,unsigned pixels);
#endif
