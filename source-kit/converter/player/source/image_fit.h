#ifndef IMAGE_FIT_H
#define IMAGE_FIT_H
#include <stdbool.h>
/* Fit the original picture inside one DS screen. Raster dimensions describe
 * the existing renderer's buffer, not the original picture's aspect ratio. */
typedef struct {unsigned x,y,width,height,scale_x,scale_y;} image_fit;
static inline bool image_fit_make_inset(image_fit *f,unsigned source_w,unsigned source_h,unsigned raster_w,unsigned raster_h,unsigned inset){
    if(!f || !source_w || !source_h || source_w>4096 || source_h>4096 ||
       !raster_w || !raster_h || raster_w>256 || raster_h>256 || inset>=128)return false;
    unsigned bound=256-2*inset,w=bound,h=192;
    if(source_w*192>=source_h*bound)h=(source_h*bound+source_w/2)/source_w;
    else w=(source_w*192+source_h/2)/source_h;
    if(!w)w=1;
    if(!h)h=1;
    f->scale_x=(raster_w*256+w-1)/w;f->scale_y=(raster_h*256+h-1)/h;
    /* Use the actual affine coverage, so fixed-point rounding cannot push
     * a picture beyond the edge or expose uninitialised bitmap rows. */
    f->width=(raster_w*256+f->scale_x-1)/f->scale_x;
    f->height=(raster_h*256+f->scale_y-1)/f->scale_y;
    f->x=(256-f->width)/2;f->y=(192-f->height)/2;return true;
}
static inline bool image_fit_make(image_fit *f,unsigned source_w,unsigned source_h,unsigned raster_w,unsigned raster_h){
    return image_fit_make_inset(f,source_w,source_h,raster_w,raster_h,0);
}
#endif
