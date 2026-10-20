#include "native_entry_display.h"
#include "native_entry.h"
#include "upper_layout.h"
#include "settings.h"
#include "video_filters.h"
#include <nds.h>
#include <string.h>
static volatile bool active;
static bool shared_clock;
extern unsigned music_player_played(void);
static unsigned background,page,timer_id,period;
static unsigned page_height[2],last_revision,sample_origin;
static volatile unsigned ticks,previous,last_sample,error;
static bool last_tv;
static volatile bool prepared;
static unsigned prepared_page,prepared_height,prepared_revision,prepared_sample,shown_height;
static bool prepared_tv;
static uint16_t prepared_palette[64];
volatile unsigned native_display_prepares,native_display_commits,native_display_late_irqs;
void native_entry_display_prepare(void);
static uint32_t row_scratch[192/4];
static unsigned page_rows[2][192];
volatile unsigned native_display_rows_uploaded,native_display_rows_reused;
static bool begin_at(unsigned bg,unsigned timer,unsigned clocks_per_sample,unsigned played,bool live){
    if(bg>=4 || timer>=4 || !clocks_per_sample)return false;
    active=false;prepared=false;shown_height=0;shared_clock=live;background=bg;timer_id=timer;period=clocks_per_sample;
    /* Clear both page borders. Index0 reveals decoration only outside the
     * original picture; its own black pixels are opaque index128. */
    dmaFillWords(0,BG_GFX,128*1024);
    memset(page_rows,0,sizeof(page_rows));
    native_display_rows_uploaded=native_display_rows_reused=0;
    sample_origin=played;page=ticks=error=0;previous=live?TIMER_DATA(timer):0;last_sample=~0u;page_height[0]=page_height[1]=0;last_revision=0;
    bgSetPriority(bg,0);
    native_display_prepares=native_display_commits=native_display_late_irqs=0;
    active=true;native_entry_display_prepare();native_entry_display_vblank();bgShow(bg);return true;
}
void native_entry_display_end(void){active=false;prepared=false;}
unsigned native_entry_display_played(void){
    if(!active)return 0;
    if(shared_clock)return music_player_played();
    int irq=enterCriticalSection();
    unsigned elapsed=ticks+((TIMER_DATA(timer_id)-previous)&65535);
    leaveCriticalSection(irq);
    return sample_origin+(uint64_t)elapsed*1024/period;
}
void native_entry_display_prepare(void){
    if(!active || prepared)return;
    unsigned played=native_entry_display_played();
    const uint8_t *pixels;const uint8_t *const *rows;const uint16_t *palette;unsigned sample;
    if(!native_entry_frame_view(played,&pixels,&rows,&palette,&sample)){error=1;return;}
    if(sample==last_sample && last_tv==settings_tv_filter)return;
    unsigned height=native_entry_picture_height(sample);
    if(!height || height>192){error=2;return;}
    unsigned revision=native_entry_pixel_revision(sample);
    bool upload=!revision || revision!=last_revision || height!=page_height[page];
    unsigned target=page;
    if(upload){
    target^=1;
    for(unsigned y=0;y<height;y++){
        unsigned identity=rows?native_entry_row_serial(rows[y]):native_entry_contiguous_row_serial(sample,y);
        if(identity && page_rows[target][y]==identity){native_display_rows_reused++;continue;}
        const uint8_t *row=rows?native_entry_row_pixels(rows[y],(uint8_t*)row_scratch):pixels+y*192;
        if(!row){error=3;return;}
        if(!rows || row==(uint8_t*)row_scratch)DC_FlushRange(row,192);
        dmaCopyWords(2,row,(uint8_t*)BG_GFX+target*65536+y*256+32,192);
        page_rows[target][y]=identity;native_display_rows_uploaded++;
    }
    if(height<page_height[target]){
        dmaFillWords(0,(uint8_t*)BG_GFX+target*65536+height*256,(page_height[target]-height)*256);
        memset(page_rows[target]+height,0,(page_height[target]-height)*sizeof(page_rows[0][0]));
    }
    page_height[target]=height;
    }
    for(unsigned i=0;i<64;i++){
        unsigned color=(settings_tv_filter?video_filters_warm(palette[i]):palette[i])&32767;
        prepared_palette[i]=color;
    }
    prepared_tv=settings_tv_filter;prepared_page=target;prepared_height=height;
    prepared_revision=revision;prepared_sample=sample;
    unsigned irq=enterCriticalSection();
    native_display_prepares++;prepared=true;
    leaveCriticalSection(irq);
}
void native_entry_display_vblank(void){
    if(!active)return;
    unsigned counter=TIMER_DATA(timer_id);
    ticks+=(counter-previous)&65535;previous=counter;
    if(!prepared)return;
    /* A long earlier IRQ/critical section can enter after visible scanout
     * resumed. Retain the complete page until the next safe VBlank. */
    unsigned line=REG_VCOUNT;
    if(line<192 || line>244){native_display_late_irqs++;return;}
    page=prepared_page;
    bgSetMapBase(background,page*4);
    for(unsigned i=0;i<64;i++){
        BG_PALETTE[128+i]=prepared_palette[i];BG_PALETTE[192+i]=prepared_palette[i];
    }
    if(shown_height!=prepared_height){
        upper_layout_apply_raster(background,192*7,prepared_height*6,32,192,prepared_height);
        shown_height=prepared_height;
    }
    last_tv=prepared_tv;last_revision=prepared_revision;last_sample=prepared_sample;
    native_display_commits++;prepared=false;
}
unsigned native_entry_display_sample(void){return last_sample;}
unsigned native_entry_display_error(void){return error;}

bool native_entry_display_begin(unsigned bg,unsigned timer,unsigned clocks){
    return begin_at(bg,timer,clocks,0,false);
}
bool native_entry_display_begin_live(unsigned bg,unsigned timer,unsigned clocks,unsigned played){
    /* Existing Maxmod stream keeps its ring, timer and unplayed sound. */
    return begin_at(bg,timer,clocks,played,true);
}
