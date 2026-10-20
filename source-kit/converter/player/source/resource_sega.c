#include "resource_sega.h"
#include "resource_background.h"
#include <string.h>
static uint8_t actors[80*64];
static uint8_t *bank;
static const uint8_t *parameters,*maps,*poses;
static resource_background_sprite sprites[80],visible[80];
static unsigned sprite_count,visible_count,current;
static uint16_t be16(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static uint32_t be32(const uint8_t *p){return (uint32_t)be16(p)<<16|be16(p+2);}
static uint16_t le16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t le32(const uint8_t *p){return le16(p)|(uint32_t)le16(p+2)<<16;}
static void w16(uint8_t *p,uint16_t x){p[0]=x>>8;p[1]=x;}
static void w32(uint8_t *p,uint32_t x){w16(p,x>>16);w16(p+2,x);}
static void reset(void){
    memset(actors,0,sizeof(actors));sprite_count=visible_count=current=0;
    for(unsigned i=0;i<80;i++){
        unsigned d7=79-i;const uint8_t *p=parameters+i*6;uint8_t *a=actors+i*64;
        w16(a,4);w16(a+8,0xd3);w16(a+12,d7<=15?0xeb:0xe4);
        a[0x23]=d7;w32(a+0x14,0u-(uint32_t)be16(p)*4);
        w16(a+0x28,be16(p+2));w32(a+0x10,(uint32_t)be16(p+4)*2-0xe00u);
        a[0x21]=(d7&0x3c)>>2;a[0x20]=d7&3;a[0x26]=d7;
    }
}
bool resource_sega_bind(void *data,size_t bytes){
    bank=NULL;
    const unsigned at=44+128+65536+60;
    if(!data || bytes<at+532)return false;
    uint8_t *p=data,*extra=p+at;
    if(memcmp(p,"NSBG",4) || le16(p+4)!=2 || le32(p+8)!=172 ||
       memcmp(extra,"NSGA",4) || le16(extra+4)!=1 || le16(extra+6)!=80 ||
       le32(extra+8)!=20 || le32(extra+12)!=500 || le32(extra+16)!=32)return false;
    if(!resource_background_bind(data,bytes) || !resource_background_view(32,16,256,192))return false;
    /* All logo, Sonic and license ink fits this native-size viewport.
     * Do not shrink 320x224: it deletes single-pixel TM/font strokes. */
    bank=p;parameters=extra+20;maps=extra+500;poses=p+44+128+65536;reset();return true;
}
/* Direct translation of BIOS6D68,1C90 and1C18. Unsigned wrapping preserves
 * original fixed-point arithmetic, byte counters and palette phases. */
static void actor_pass(unsigned first,unsigned count){
    sprite_count=0;
    for(unsigned i=first;i<first+count;i++){
        uint8_t *a=actors+i*64;unsigned timer=be16(a+0x28);
        if(timer){
            w32(a+0x14,be32(a+0x14)+0x800u);w16(a+0x28,--timer);
            if(!timer){w32(a+0x10,0);w32(a+0x14,0);}
        }
        a[0x26]+=8;unsigned phase=a[0x26]&0x60;a[0x18]=phase==0x60?0:phase;
        if((int8_t)--a[0x21]<0){a[0x21]=8;++a[0x20];}
        if(a[0x20]>=4)a[0x20]=0;
        w32(a+4,0x6ad0u+a[0x20]*8);const uint8_t *map=maps+a[0x20]*8;
        w32(a+8,be32(a+8)+be32(a+0x10));w32(a+12,be32(a+12)+be32(a+0x14));
        a[0x19]=map[1];
        unsigned x=be16(a+8),y=be16(a+12);
        if(y>0x180 || (uint16_t)(x+(int8_t)map[6]-0x41)>=0x17f)continue;
        resource_background_sprite *s=&sprites[sprite_count++];
        s->x=((x+(int8_t)map[6])&511)-128;
        s->y=((y+(int8_t)map[2])&511)-128;
        s->size=(uint16_t)map[3]<<8;
        s->attribute=(uint16_t)(map[4]|a[0x18])<<8|map[5];
    }
}
const uint8_t *resource_sega_actor_state(void){return bank?actors:NULL;}
bool resource_sega_draw(unsigned frame,uint16_t output[256*192]){
    if(!bank || !output || frame>=RESOURCE_SEGA_FRAMES)return false;
    if(frame<current)reset();
    while(current<frame){
        ++current;
        if(current>=118){
            /* VBlank uploads the previous SAT before the CPU updates actors. */
            memcpy(visible,sprites,sprite_count*sizeof(*sprites));visible_count=sprite_count;
            actor_pass(current<122?64:0,current<122?16:80);
        }
    }
    unsigned pose=frame<=60?0:((((frame<118?frame-60:57)*26)&128)?0:1);
    uint8_t *vram=bank+172;
    for(unsigned y=0;y<5;y++)for(unsigned x=0;x<3;x++){
        uint8_t *cell=vram+0xc000+((11+y)*64+8+x)*2;
        unsigned value=be16(poses+(pose*15+y*3+x)*2);
        if(be16(cell)!=value){w16(cell,value);resource_background_map_changed(0,8+x,11+y,1,1);}
    }
    return resource_background_sprites(visible,visible_count) && resource_background_draw(output);
}
