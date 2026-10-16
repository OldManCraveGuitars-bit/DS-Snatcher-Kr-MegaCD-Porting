#include "upper_backdrop.h"
#include "resource_background.h"
#include <nds.h>
#include <filesystem.h>
#include <stdio.h>
#include <string.h>
static uint16_t *pixels;
/* DMA cannot read the DS CPU's DTCM stack. Keep the upload row in main RAM. */
static uint16_t upload_row[256] __attribute__((aligned(32)));
volatile uint32_t upper_backdrop_state[8];
bool upper_backdrop_begin(void){
    if(!nitroFSInit(NULL))return false;
    FILE *f=fopen("nitro:/junker-upper-r1.rgb555","rb");if(!f)return false;
    vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
    int bg=bgInitHidden(2,BgType_Bmp16,BgSize_B16_256x256,8,0);
    pixels=(uint16_t*)bgGetGfxPtr(bg);
    bool good=true;
    for(unsigned y=0;y<192;y++){
        if(fread(upload_row,2,256,f)!=256){good=false;break;}
        DC_FlushRange(upload_row,sizeof(upload_row));
        dmaCopyWords(2,upload_row,pixels+y*256,sizeof(upload_row));
    }
    if(fgetc(f)!=EOF || ferror(f))good=false;
    fclose(f);
    if(!good){pixels=NULL;bgHide(bg);return false;}
    bgSetPriority(bg,3);bgSetPriority(3,0);bgSetScale(bg,256,256);bgSetScroll(bg,0,0);bgUpdate();
    bgShow(bg);
    upper_backdrop_state[0]=(uintptr_t)pixels;
    upper_backdrop_state[1]=REG_DISPCNT;upper_backdrop_state[2]=REG_BG2CNT;
    upper_backdrop_state[3]=pixels[0];upper_backdrop_state[4]=pixels[10000];
    upper_backdrop_state[5]=REG_BG2PA;upper_backdrop_state[6]=REG_BG2PD;
    upper_backdrop_state[7]=VRAM_B_CR;
    /* TV blur uses both affine bitmap layers. A tiled copy of the approved
     * casing provides the exterior in that optional mode only. */
    vramSetBankD(VRAM_D_MAIN_BG_0x06040000);
    REG_DISPCNT|=DISPLAY_CHAR_BASE(4)|DISPLAY_SCREEN_BASE(4);
    int tiles=bgInitHidden(0,BgType_Text8bpp,BgSize_T_256x256,24,0);bgSetPriority(tiles,3);
    FILE *tv=fopen("nitro:/junker-upper-tv-r22.bin","rb");if(!tv)return false;
    uint16_t palette[128];bool tv_good=fread(palette,2,128,tv)==128;
    if(tv_good)for(unsigned i=0;i<128;i++)BG_PALETTE[i]=palette[i];
    uint8_t *gfx=(uint8_t*)bgGetGfxPtr(tiles);
    for(unsigned at=0;at<49152 && tv_good;at+=512){
        tv_good=fread(upload_row,1,512,tv)==512;DC_FlushRange(upload_row,512);dmaCopyWords(2,upload_row,gfx+at,512);
    }
    if(fgetc(tv)!=EOF || ferror(tv))tv_good=false;
    fclose(tv);if(!tv_good)return false;
    uint16_t *map=bgGetMapPtr(tiles);for(unsigned i=0;i<1024;i++)map[i]=i<768?i:0;
    /* Decorative backing is outside the game image. Original black pixels,
     * including its backdrop and fades, remain part of the opaque image. */
    resource_background_ui_backdrop(false);return true;
}
bool upper_backdrop_draw(uint16_t output[256*192]){
    if(!pixels || !output)return false;
    memcpy(output,pixels,256*192*2);return true;
}

bool upper_backdrop_draw_rows(uint16_t *output,unsigned first,unsigned last){
    if(!pixels || !output || first>last || last>192)return false;
    memcpy(output+first*256,pixels+first*256,(last-first)*512);return true;
}
