#include "resource_office_motion.h"
#include "resource_actor.h"
#include <string.h>

static const uint8_t *bank;
static size_t bank_size;
static uint32_t directory,lookup,lookup_count,resource_offset,resource_size,base;
static uint8_t actors[64*80];
static resource_actor_clock clock;
static unsigned elapsed,last_voice;
static bool initialized;
static const unsigned eye_ids[3]={8,5,11};
static const unsigned mouth_ids[3]={9,6,12};
static const unsigned body_ids[3]={10,7,13};
static uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t)p[1]<<8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | (uint32_t)le16(p+2)<<16; }
static uint16_t be16(const uint8_t *p) { return (uint16_t)p[0]<<8 | p[1]; }
static uint32_t be32(const uint8_t *p) { return (uint32_t)be16(p)<<16 | be16(p+2); }
static bool range(uint32_t at,size_t bytes) { return at<=bank_size && bytes<=bank_size-at; }
static bool source_range(uint32_t address,size_t bytes) {
    return address>=base && address-base<=resource_size && bytes<=resource_size-(address-base);
}
bool resource_office_bind(const void *data,size_t size) {
    bank=NULL;initialized=false;
    if(!data || size<32)return false;
    const uint8_t *p=data;
    if(memcmp(p,"NSAR",4) || le16(p+4)!=1 || le16(p+6)!=9)return false;
    bank_size=size;directory=le32(p+8);lookup=le32(p+12);lookup_count=le32(p+16);
    resource_offset=le32(p+20);resource_size=le32(p+24);base=le32(p+28);
    if(!range(directory,9*12) || lookup_count!=27 || !range(lookup,27*16)
       || !range(resource_offset,resource_size) || base!=0x28000 || !resource_size)return false;
    for(unsigned i=0;i<9;i++) {
        const uint8_t *voice=p+directory+i*12;
        if(le16(voice)!=86+i || voice[2]!=4+i || voice[3]!=3 || le32(voice+8))return false;
        unsigned offset=le32(voice+4);
        if(!range(offset,3*84))return false;
        for(unsigned j=0;j<3;j++) {
            const uint8_t *a=p+offset+j*84;
            if(a[0]<5 || a[0]>13 || a[1] || a[2] || a[3] || be16(a+4)!=1)return false;
            if(!source_range(be32(a+4+0x32),4) || !source_range(be32(a+4+0x2e),2)
               || !source_range(be32(a+4+0x36),2))return false;
        }
    }
    for(unsigned i=0;i<27;i++) {
        const uint8_t *entry=p+lookup+i*16;
        if(entry[0]>=3 || entry[1]>=70 || le16(entry+2)
           || !source_range(le32(entry+4),2) || !source_range(le32(entry+8),2)
           || !source_range(le32(entry+12),2))return false;
    }
    bank=p;return true;
}
static bool pictures(uint16_t poses[3]) {
    for(unsigned slot=0;slot<3;slot++) {
        const uint8_t *body=actors+body_ids[slot]*80;
        poses[slot]=65535;
        if(be16(body)!=1)continue;
        uint32_t eyes=be32(actors+eye_ids[slot]*80+0x36);
        uint32_t mouth=be32(actors+mouth_ids[slot]*80+0x36);
        uint32_t body_map=be32(body+0x36);
        for(unsigned i=0;i<lookup_count;i++) {
            const uint8_t *entry=bank+lookup+i*16;
            if(entry[0]==slot && le32(entry+4)==eyes && le32(entry+8)==mouth
               && le32(entry+12)==body_map) { poses[slot]=entry[1];break; }
        }
        if(poses[slot]==65535)return false;
    }
    return true;
}
static void introduce(unsigned index,bool previous) {
    const uint8_t *voice=bank+directory+index*12;
    unsigned offset=le32(voice+4);
    for(unsigned j=0;j<3;j++) {
        const uint8_t *entry=bank+offset+j*84;
        uint8_t *a=actors+entry[0]*80;
        memcpy(a,entry+4,80);
        if(previous && (entry[0]==6 || entry[0]==9 || entry[0]==12)) {
            a[0x25]|=1; a[0x28]=a[0x29]=0;
        }
    }
}
static bool tick(void) {
    /* The original engine also invokes FD96 once before stepping actors.
     * Shared RNG consumption from the rest of the game is not yet ported. */
    resource_actor_random(&clock.rng);
    return resource_actor_tick(actors,bank+resource_offset,resource_size,base,
                               &clock,UINT64_C(0x3fe0));
}
bool resource_office_begin(unsigned voice,uint16_t poses[3]) {
    if(!bank || !poses || voice>=9)return false;
    if(!initialized || voice==0 || voice<last_voice) {
        memset(actors,0,sizeof(actors));memset(&clock,0,sizeof(clock));
        clock.rng=0x534e4453;clock.iterations=1;
        for(unsigned i=0;i<voice;i++)introduce(i,true);
        initialized=true;
    }
    introduce(voice,false);clock.pcm_flags=0;elapsed=0;last_voice=voice;
    if(!tick())return false;
    return pictures(poses);
}
bool resource_office_update(unsigned samples,unsigned sample_rate,uint16_t poses[3]) {
    if(!bank || !initialized || !sample_rate || !poses)return false;
    /* NTSC source VBlank = 59.9227434 Hz. This uses the actual DS PCM timer;
     * it does not play a recorded list of portrait changes. */
    unsigned target=(uint64_t)samples*59922743/((uint64_t)sample_rate*1000000)+1;
    if(target<elapsed || target-elapsed>1024)return false;
    clock.pcm_flags=2;
    while(elapsed<target) {
        if(!tick())return false;
        elapsed++;
    }
    return pictures(poses);
}
