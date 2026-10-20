#include "resource_pcm_effect.h"
#include <string.h>
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
bool resource_pcm_effect_lookup(const resource_pcm_effect_bank *b,unsigned command,resource_pcm_effect *e){
    if(!b || !e || !b->table || b->table_bytes<0x4010 || !command || command>=b->table[0x4000])return false;
    size_t at=0x4000+command*16;
    if(at>b->table_bytes || b->table_bytes-at<16)return false;
    const uint8_t *p=b->table+at;uint32_t address=longword(p+6),base;
    const uint8_t *data;size_t bytes;
    if(address>=0x55000 && address<0x65000){base=0x55000;data=b->resident;bytes=b->resident_bytes;}
    else if(address>=0x65000 && address<0x7f800){base=0x65000;data=b->dynamic;bytes=b->dynamic_bytes;}
    else return false;
    size_t offset=address-base;
    if(!data || offset>bytes || bytes-offset<16)return false;
    const uint8_t *header=data+offset;uint32_t n=longword(header);
    if(!n || n>bytes-offset-16)return false;
    const uint8_t *wave=header+16,*end=memchr(wave,255,n);
    if(!end)return false;
    for(const uint8_t *q=end;q<wave+n;q++)if(*q!=255)return false;
    *e=(resource_pcm_effect){p[0],p[1],p[2],p[3],word(p+4),word(header+8),
        address,longword(header+4),n,(uint32_t)(end-wave),wave};
    return true;
}
