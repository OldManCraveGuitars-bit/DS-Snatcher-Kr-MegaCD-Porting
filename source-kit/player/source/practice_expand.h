#ifndef PRACTICE_EXPAND_H
#define PRACTICE_EXPAND_H
#include <stdint.h>
void practice_expand_rgb555(const uint8_t *indices,const uint16_t *palette,uint16_t *output);
#endif

#include <stdbool.h>
extern unsigned char practice_dirty_rows[192];
extern volatile unsigned practice_rows_expanded,practice_rows_transferred;
bool practice_expand_rgb555_changed(const uint8_t *,const uint16_t *,uint16_t *,bool);

bool practice_expand_rgb555_changed_rows(const uint8_t *,const uint16_t *,uint16_t *,bool,const bool *);
