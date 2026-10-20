#include "ui_theme.h"
#include <stddef.h>
#include "../generated/ui_theme_indexed.h"
typedef uint32_t theme_word __attribute__((may_alias));
void ui_theme_copy(uint16_t *out,bool panel,unsigned first,unsigned pixels){
    if(!out || first>49152 || pixels>49152-first)return;
    const uint8_t *in=(panel?touch_panel_background_indices:touch_background_indices)+first;
    if(pixels && ((uintptr_t)out&3)){*out++=ui_theme_palette[*in++];pixels--;}
    while(pixels>=2){
        *(theme_word*)out=ui_theme_palette[in[0]]|((uint32_t)ui_theme_palette[in[1]]<<16);
        out+=2;in+=2;pixels-=2;
    }
    if(pixels)*out=ui_theme_palette[*in];
}
