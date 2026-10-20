#include "resource_frame.h"
typedef uint32_t alias_word __attribute__((may_alias));
void resource_frame_store(uint8_t *out,const uint8_t *raster,unsigned height){
    for(unsigned y=0;y<height;y++){
        unsigned first=(y*192+height-1)/height;
        const alias_word *src=(const alias_word *)(raster+first*256+32);
        alias_word *dst=(alias_word *)(out+y*192);
        for(unsigned x=0;x<48;x++)dst[x]=src[x]|0x80808080u;
    }
}

void resource_frame_store_native(uint8_t *out,const uint8_t *raster,unsigned height){
    for(unsigned y=0;y<height;y++){
        const alias_word *src=(const alias_word *)(raster+y*256+32);
        alias_word *dst=(alias_word *)(out+y*192);
        for(unsigned x=0;x<48;x++)dst[x]=src[x]|0x80808080u;
    }
}
