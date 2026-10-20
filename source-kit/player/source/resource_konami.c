#include "resource_konami.h"
#include "resource_background.h"
#include <string.h>
static uint8_t state[4096];
static const uint8_t *source_palette;
static unsigned music;
static resource_background_sprite beam;
static uint16_t be16(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static uint32_t le32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint16_t word(unsigned p){return be16(state+p);}
static void set(unsigned p,unsigned v){state[p]=v>>8;state[p+1]=v;}
static void palette_job(unsigned n){
    for(unsigned i=0;i<4;i++){
        unsigned choice=(n>>((3-i)*8))&255;set(0xe4+i*2,choice);
        memcpy(state+0xb80+i*32,source_palette+choice*32,32);
    }
    state[0xe0]|=1;
}
bool resource_konami_bind(const void *data,size_t bytes){
    source_palette=NULL;music=0;
    const uint8_t *p=data;unsigned at=172+65536;
    if(!data || bytes<at+240 || memcmp(p,"NSBG",4) || le32(p+8)!=172 ||
       memcmp(p+at,"NKON",4) || be16(p+at+4)!=0x0100 ||
       be16(p+at+6)!=0x0700 || le32(p+at+8)!=12)return false;
    if(!resource_background_bind(data,bytes))return false;
    source_palette=p+at+12;memset(state,0,sizeof(state));palette_job(0x04040405);
    set(0x4a,4);set(0xd00,0x88);set(0xd02,0x0500);set(0xd04,0xe004);set(0xd06,0x80);
    return true;
}
const uint8_t *resource_konami_state(void){return source_palette?state:NULL;}
unsigned resource_konami_music_command(void){unsigned result=music;music=0;return result;}
int resource_konami_tick(uint8_t clock){
    if(!source_palette)return -1;
    state[0x21]=clock;
    unsigned phase=word(0x4a),wait=word(0x4c),extent=word(0x4e);
    switch(phase){
    case 4:{
        state[0xd05]=source_palette[224+((clock>>1)&3)];set(0xd06,word(0xd06)+6);
        unsigned h=(word(0x700)+6)&511;set(0x700,h);
        if(h>=256){set(0x700,256);set(0x4c,16);set(0x4e,5);set(0x4a,5);}
        break;}
    case 5:
        set(0x4c,wait-1);
        if((int16_t)word(0x4c)<0)set(0x4a,6);
        else if(!(wait&7)){
            for(unsigned i=0;i<=extent;i++)set(0xbf0-i*2,word(0xbf0-i*2)+32);
            set(0x4e,extent+1);state[0xe0]|=1;
        }
        break;
    case 6:{
        unsigned v=(word(0xb00)-2)&1023;set(0xb00,v);
        if((int16_t)v<0x318){set(0x4a,7);set(0x4e,30);
            memcpy(state+0xba0,source_palette+32,64);music=0x3f;}
        break;}
    case 7:
        if(!(clock&1)){
            state[0xe0]|=1;unsigned n=(32-(extent&255))/2;
            for(unsigned i=0;i<n;i++){
                set(0xba0+(extent&255)+i*2,be16(source_palette+44+i*2));
                set(0xbc0+(extent&255)+i*2,be16(source_palette+76+i*2));
            }
            set(0x4e,extent-2);
            if(word(0x4e)==10){set(0x4e,30);set(0x4a,8);
                memcpy(state+0xb80,source_palette,32);set(0xb80,0);}
        }
        break;
    case 8:
        if(!(clock&1)){
            state[0xe0]|=1;unsigned n=(32-(extent&255))/2;
            for(unsigned i=0;i<n;i++)set(0xb80+(extent&255)+i*2,be16(source_palette+12+i*2));
            set(0x4e,extent-2);
            if(word(0x4e)==10){set(0x4e,0);set(0x4c,3);set(0x4a,9);}
        }
        break;
    case 9:
        set(0x4c,wait-1);
        if(!word(0x4c)){
            set(0x4c,3);state[0xe0]|=1;set(0xb80,word(0xb80)+0x222);
            if(word(0xb80)==0xeee){palette_job(0x00010203);set(0x4e,512);set(0x4a,10);}
        }
        break;
    case 10:
        set(0x4e,extent-1);if(!word(0x4e)){set(0x4e,40);set(0x4a,11);}break;
    case 11:
        set(0x4e,extent-1);
        if(!word(0x4e)){set(0x700,0);set(0x702,0);set(0xb00,0);set(0xb02,0);set(0x26,1);return 1;}
        if(!(word(0x4e)&1)){
            state[0xe0]|=1;
            for(unsigned i=0;i<64;i++){
                unsigned color=word(0xb80+i*2),r=color&14,g=color&224,b=color&3584;
                set(0xb80+i*2,(r?r-2:0)|(g?g-32:0)|(b?b-512:0));
            }
        }
        break;
    default:return -1;
    }
    return 0;
}
static bool prepare_picture(void){
    if(!source_palette)return false;
    uint16_t palette[64];for(unsigned i=0;i<64;i++)palette[i]=word(0xb80+i*2);
    beam.x=(word(0xd06)&511)-128;beam.y=(word(0xd00)&511)-128;
    beam.size=word(0xd02);beam.attribute=word(0xd04);
    return resource_background_palette(0,palette,64) &&
        resource_background_scroll(0,word(0x700),-(int)word(0xb00)) &&
        resource_background_sprites(&beam,1);
}
bool resource_konami_draw(uint16_t output[256*192]){
    return output && prepare_picture() && resource_background_draw(output);
}
bool resource_konami_draw_indexed(uint8_t output[256*192]){
    return output && prepare_picture() && resource_background_draw_indexed8_opaque(output);
}
