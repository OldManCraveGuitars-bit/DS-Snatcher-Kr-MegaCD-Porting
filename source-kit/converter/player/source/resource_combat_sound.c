#include "resource_combat_sound.h"
#include <string.h>
bool resource_combat_sound_bind(resource_combat_sounds *out,const void *data,size_t bytes,unsigned stage){
    const uint8_t *sub=data;
    /* Current combat VM implements the original first encounter only.
     * Later stage-specific branches must be decoded before enabling them. */
    if(!out || !sub || stage!=1 || bytes<0x12e70-0xd400)return false;
    const unsigned table[]={0x11f10,0x11f30,0x11f40,0x11f20};
    uint8_t commands[4];
    for(unsigned i=0;i<4;i++){
        commands[i]=sub[table[i]-0xd400+stage];
        if(!commands[i] || commands[i]>=0xfc)return false;
    }
    /* Source damage path12E66 loads D0, clears D1 and calls FM150FA.
     * Read its immediate command rather than confusing it with a PCM ID. */
    const uint8_t *p=sub+0x12e66-0xd400;
    if(p[0]!=0x70 || memcmp(p+2,"\x72\0\x4e\xb9\0\x01\x50\xfa",8))return false;
    resource_combat_sounds result={commands[0],commands[1],commands[2],commands[3],p[1]};
    *out=result;return true;
}
