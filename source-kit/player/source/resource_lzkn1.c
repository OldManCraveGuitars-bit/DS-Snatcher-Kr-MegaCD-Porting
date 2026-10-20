#include "resource_lzkn1.h"
bool resource_lzkn1(const uint8_t *data,size_t size,uint8_t *out,size_t capacity,size_t *decoded,size_t *consumed){
    if(!data || !out || !decoded || !consumed || size<3)return false;
    size_t expected=(unsigned)data[0]<<8|data[1],at=2,pos=0;
    unsigned flags=0,bits=0;
    if(expected>capacity)return false;
    for(;;){
        if(!bits){if(at>=size)return false;flags=data[at++];bits=8;}
        unsigned token=flags&1;flags>>=1;bits--;
        if(at>=size)return false;
        unsigned code=data[at++];
        if(!token){if(pos>=expected)return false;out[pos++]=code;continue;}
        if(code==31)break;
        unsigned length,distance;
        if(code<128){
            if(at>=size)return false;
            length=(code&31)+3;distance=((code<<3)&0x300)|data[at++];
        }else if(code<192){length=(code>>4)-6;distance=code&15;}
        else{
            length=code-0xb8;
            if(length>size-at || length>expected-pos)return false;
            for(unsigned i=0;i<length;i++)out[pos++]=data[at++];
            continue;
        }
        if(!distance || distance>pos || length>expected-pos)return false;
        for(unsigned i=0;i<length;i++){out[pos]=out[pos-distance];pos++;}
    }
    if(pos!=expected)return false;
    *decoded=pos;*consumed=at;return true;
}
