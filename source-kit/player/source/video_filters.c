#include "video_filters.h"
#include "settings.h"
#include <nds.h>
extern volatile unsigned upper_layout_state[8];
static uint16_t lines[2][192] __attribute__((aligned(32)));
static uint16_t warm_red[32],warm_blue[32];
static unsigned previous=~0u;
static bool practice_view;
void video_filters_practice(bool active){if(active!=practice_view){practice_view=active;previous=~0u;}}
volatile unsigned video_filter_state[12];
void video_filters_init(void){
    /* The original RGB555 lookup is separable: green and opacity do not
     * change; red and blue each depend only on their own five bits.
     * Keep exactly the same colors in 128 bytes instead of 65536. */
    for(unsigned i=0;i<32;i++){
        warm_red[i]=i+(i && i<30?1:0);
        warm_blue[i]=((i*7+4)/8)<<10;
    }
}
uint16_t video_filters_warm(uint16_t color){
    return (color&0x83e0u)|warm_red[color&31]|warm_blue[(color>>10)&31];
}
void video_filters_copy(const uint16_t *source,uint16_t *destination,unsigned count){
    for(unsigned i=0;i<count;i++)destination[i]=video_filters_warm(source[i]);
}
void video_filters_vblank(bool upper,bool lower_combat){
    video_filter_state[0]=REG_BLDALPHA;video_filter_state[1]=REG_BLDY;
    bool tv=settings_tv_filter,scan=settings_scanlines;
    /* Native dots remain unblended unless the user enables TV filtering. */
    unsigned flags=upper|lower_combat<<1|tv<<2|scan<<3;
    dmaStopSafe(0);dmaStopSafe(1);
    if(flags!=previous){
        for(unsigned row=0;row<192;row++){
            unsigned next=row+1;
            bool dark=scan && next<192 && (next&1);
            /* One dark display pixel alternating with one light pixel is
             * the finest DS scanline pitch. Reduce BOTH alpha weights on
             * dark rows so the TV trail cannot fill the scanline back in. */
            unsigned value=tv?(dark?6|(4<<8):10|(6<<8)):(dark?10:16);
            lines[0][row]=value;lines[1][row]=!practice_view && next<17?16:value;
        }
        DC_FlushRange(lines,sizeof(lines));previous=flags;
    }
    if((tv || scan) && upper){
        /* A second bitmap view shares the original pixels, displaced by
         * one display pixel. Only TV mode mixes its3/8 contribution.
         * Scanline-only mode gives the neighbour zero weight, preserving
         * dot boundaries while darkening alternating rows in hardware. */
        REG_BG2CNT=(REG_BG3CNT&~3)|1;
        /* All affine parameters are write-only, including PA..PD. This
         * presenter uses zero rotation/centre; clone its SDK scale/scroll. */
        REG_BG2PA=bgState[3].scaleX;REG_BG2PD=bgState[3].scaleY;
        REG_BG2PB=REG_BG2PC=0;
        REG_BG2X=bgState[3].scrollX-(tv?bgState[3].scaleX:0);REG_BG2Y=bgState[3].scrollY;
        bgShow(0);bgShow(2);
        /* Confine the displaced bitmap to the original picture rectangle.
         * Two windows handle the full256-pixel width (an8-bit right edge
         * cannot encode256 without wrapping). The casing stays unblended. */
        unsigned x=upper_layout_state[2],y=upper_layout_state[3];
        unsigned w=upper_layout_state[4],h=upper_layout_state[5];
        WIN0_X0=x;WIN0_X1=w==256?255:x+w;
        WIN1_X0=w==256?255:x;WIN1_X1=w==256?0:x+w;
        WIN0_Y0=WIN1_Y0=y;WIN0_Y1=WIN1_Y1=y+h;
        REG_WININ=0x2d2d;REG_WINOUT=1;
        REG_DISPCNT|=DISPLAY_WIN0_ON|DISPLAY_WIN1_ON;
    }else{
        REG_DISPCNT&=~(DISPLAY_WIN0_ON|DISPLAY_WIN1_ON);
        REG_BG2CNT=BG_BMP16_256x256|BG_BMP_BASE(8)|3;
        REG_BG2PA=REG_BG2PD=256;REG_BG2PB=REG_BG2PC=0;REG_BG2X=REG_BG2Y=0;
        bgHide(0);bgShow(2);
    }
    if((tv || scan) && lower_combat){
        REG_BG2CNT_SUB=(REG_BG3CNT_SUB&~3)|1;
        REG_BG2PA_SUB=bgState[7].scaleX;REG_BG2PD_SUB=bgState[7].scaleY;
        REG_BG2PB_SUB=REG_BG2PC_SUB=0;
        REG_BG2X_SUB=bgState[7].scrollX-(tv?bgState[7].scaleX:0);REG_BG2Y_SUB=bgState[7].scrollY;
        REG_DISPCNT_SUB|=DISPLAY_BG2_ACTIVE;
        REG_DISPCNT_SUB&=~DISPLAY_WIN0_ON;
    }else REG_DISPCNT_SUB&=~(DISPLAY_BG2_ACTIVE|DISPLAY_WIN0_ON);
    unsigned control=BLEND_ALPHA|BLEND_SRC_BG3|BLEND_DST_BG2;
    REG_BLDCNT=upper && (tv || scan)?control:0;
    REG_BLDCNT_SUB=lower_combat && (tv || scan)?control:0;
    REG_BLDY=REG_BLDY_SUB=0;REG_BLDALPHA=tv?10|(6<<8):16;REG_BLDALPHA_SUB=practice_view?(tv?10|(6<<8):16):16;
    /* HBlank DMA, not one CPU interrupt per line. Channels2/3 remain
     * available to the original bitmap presenter and ordinary uploads. */
    if(upper && scan)dmaSetParams(0,lines[0],(void*)&REG_BLDALPHA,DMA_ENABLE|DMA_REPEAT|DMA_DST_FIX|DMA_START_HBL|1);
    if(lower_combat && (scan || tv))dmaSetParams(1,lines[1],(void*)&REG_BLDALPHA_SUB,DMA_ENABLE|DMA_REPEAT|DMA_DST_FIX|DMA_START_HBL|1);
    video_filter_state[2]=REG_DISPCNT;video_filter_state[3]=REG_BG2CNT;
    video_filter_state[4]=REG_BG3CNT;video_filter_state[5]=REG_BLDCNT;
    video_filter_state[6]=REG_WININ;video_filter_state[7]=REG_WINOUT;
    video_filter_state[8]=bgState[3].scaleX;video_filter_state[9]=bgState[3].scaleY;
    video_filter_state[10]=bgState[3].scrollX;video_filter_state[11]=bgState[3].scrollY;
}
