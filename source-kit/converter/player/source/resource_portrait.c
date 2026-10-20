#include "resource_portrait.h"
#include <string.h>

static const uint8_t *resource;
static size_t resource_bytes;
static unsigned frames, tiles, pattern_offset;
static unsigned le16(const uint8_t *p){return p[0]|p[1]<<8;}
static unsigned le32(const uint8_t *p){return le16(p)|le16(p+2)<<16;}
static unsigned be16(const uint8_t *p){return p[0]<<8|p[1];}
static bool range(size_t at,size_t length){return at<=resource_bytes && length<=resource_bytes-at;}
static uint16_t color(unsigned raw){
    unsigned r=raw&15,g=raw>>4&15,b=raw>>8&15;
    r=r*2|(r>>3);g=g*4|(g>>2);b=b*2|(b>>3);
    r=r*255/31;g=g*255/63;b=b*255/31;
    return 0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10);
}
unsigned resource_portrait_count(void){return frames;}
bool resource_portrait_bind(const void *data,size_t bytes){
    resource=NULL;resource_bytes=0;frames=tiles=pattern_offset=0;
    if(!data || bytes<16 || memcmp(data,"NSPR",4))return false;
    const uint8_t *p=data;
    if(le16(p+4)!=1)return false;
    unsigned n=le16(p+6),t=le32(p+8),at=le32(p+12);
    if(!n || n>256 || !t || t>65535 || at<16+n*4 || at>bytes || t>(bytes-at)/32 || at+t*32!=bytes)return false;
    resource=p;resource_bytes=bytes;frames=n;tiles=t;pattern_offset=at;
    for(unsigned f=0;f<n;f++){
        unsigned start=le32(p+16+f*4);
        if(start<16+n*4 || !range(start,132) || start>=at)goto invalid;
        unsigned count=le16(p+start);
        if(!count || count>64 || !range(start+132,count*12) || start+132+count*12>at)goto invalid;
        for(unsigned s=0;s<count;s++){
            const uint8_t *item=p+start+132+s*12;
            unsigned w=item[4],h=item[5],refs=le32(item+8);
            if(!w || w>4 || !h || h>4 || item[6]>3 || item[7]>3 || refs<132+count*12 || refs>at-start || w*h*2>at-start-refs || !range((size_t)start+refs,w*h*2))goto invalid;
            for(unsigned i=0;i<w*h;i++)if(le16(p+start+refs+i*2)>=t)goto invalid;
        }
    }
    return true;
invalid:
    resource=NULL;resource_bytes=0;frames=tiles=pattern_offset=0;return false;
}
bool resource_portrait_draw(unsigned frame,uint16_t pixels[4096]){
    if(!resource || frame>=frames || !pixels)return false;
    const uint8_t *start=resource+le32(resource+16+frame*4);
    unsigned count=le16(start);uint16_t palette[64];
    for(unsigned i=0;i<64;i++)palette[i]=color(be16(start+4+i*2));
    for(unsigned i=0;i<4096;i++)pixels[i]=0x8000;
    // Reverse SAT order: frontmost original sprite is painted last.
    for(unsigned s=count;s--;){
        const uint8_t *item=start+132+s*12;
        int sx=(int16_t)le16(item),sy=(int16_t)le16(item+2);
        unsigned w=item[4]*8,h=item[5]*8,bank=item[6]*16,flips=item[7];
        const uint8_t *refs=start+le32(item+8);
        for(unsigned y=0;y<h;y++){
            int dy=sy+(int)y;if(dy<0 || dy>=64)continue;
            for(unsigned x=0;x<w;x++){
                int dx=sx+(int)x;if(dx<0 || dx>=64)continue;
                unsigned tx=flips&1?w-1-x:x,ty=flips&2?h-1-y:y;
                unsigned tile=le16(refs+((tx/8)*(h/8)+ty/8)*2);
                uint8_t packed=resource[pattern_offset+tile*32+(ty&7)*4+(tx&7)/2];
                unsigned index=tx&1?packed&15:packed>>4;
                if(index)pixels[dy*64+dx]=palette[bank+index];
            }
        }
    }
    return true;
}

bool resource_portrait_draw_data(const void *data,size_t bytes,unsigned frame,uint16_t pixels[4096]){
 const uint8_t *saved=resource;size_t saved_bytes=resource_bytes;
 unsigned saved_frames=frames,saved_tiles=tiles,saved_offset=pattern_offset;
 bool good=resource_portrait_bind(data,bytes) && resource_portrait_draw(frame,pixels);
 resource=saved;resource_bytes=saved_bytes;frames=saved_frames;tiles=saved_tiles;pattern_offset=saved_offset;
 return good;
}
