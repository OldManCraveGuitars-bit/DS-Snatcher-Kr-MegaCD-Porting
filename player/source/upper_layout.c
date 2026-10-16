#include "upper_layout.h"
#include "image_fit.h"
#include <nds.h>
volatile unsigned upper_layout_state[8];
void upper_layout_apply(unsigned bg,unsigned w,unsigned h,unsigned raster_x,unsigned raster_width){
    upper_layout_apply_raster(bg,w,h,raster_x,raster_width,192);
}
void upper_layout_apply_raster(unsigned bg,unsigned w,unsigned h,unsigned raster_x,unsigned raster_width,unsigned raster_height){
    /* User requested a slight exterior side margin around illustrations.
     * Keep original pixel aspect; native English-card ink is unchanged. */
    image_fit f;if(!image_fit_make_inset(&f,w,h,raster_width,raster_height,(w==192*7 || w==224*7)?4:0))return;
    bgWrapOff(bg);bgSetScale(bg,f.scale_x,f.scale_y);
    bgSetScrollf(bg,(int)(raster_x*256)-(int)(f.x*f.scale_x),-(int)(f.y*f.scale_y));
    bgUpdate();
    upper_layout_state[0]=w;upper_layout_state[1]=h;
    upper_layout_state[2]=f.x;upper_layout_state[3]=f.y;
    upper_layout_state[4]=f.width;upper_layout_state[5]=f.height;
    upper_layout_state[6]=f.scale_x;upper_layout_state[7]=f.scale_y;
}
