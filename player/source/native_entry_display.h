#ifndef NATIVE_ENTRY_DISPLAY_H
#define NATIVE_ENTRY_DISPLAY_H
#include <stdbool.h>
/* Main-engine8-bit bitmap, VRAM pages at0/64KiB. Call after Maxmod starts
 * its manual timer, invoke vblank from the game's IRQ handler, and stop
 * before freeing native_entry. DMA2 is reserved for presentation. */
bool native_entry_display_begin(unsigned bg,unsigned timer,unsigned clocks_per_sample);
void native_entry_display_end(void);
void native_entry_display_vblank(void);
unsigned native_entry_display_played(void);
unsigned native_entry_display_sample(void);
unsigned native_entry_display_error(void);
#endif

bool native_entry_display_begin_live(unsigned,unsigned,unsigned,unsigned);

/* Foreground only. Completed page publication remains in VBlank. */
void native_entry_display_prepare(void);
