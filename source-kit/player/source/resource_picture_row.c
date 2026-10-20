#include "resource_picture_row.h"
#include <string.h>
unsigned resource_picture_row_pack(const uint8_t in[192],uint8_t out[193]){
    unsigned at=1;out[0]=2;
    for(unsigned x=0;x<192;){
        /* Every byte in a run equals its first byte, so validating here
         * removes a separate192-byte scan without weakening raw escapes. */
        if((in[x]&0xc0)!=0x80){out[0]=0;memcpy(out+1,in,192);return 193;}
        unsigned run=1;while(x+run<192 && in[x+run]==in[x])run++;
        out[at++]=(in[x]&63)|((run<4?run-1:3)<<6);
        if(run>=4)out[at++]=run-4;
        x+=run;
    }
    if(at<145)return at;
    out[0]=1;
    for(unsigned x=0;x<192;x+=4){
        unsigned v=(in[x]&63)|((in[x+1]&63)<<6)|((in[x+2]&63)<<12)|((in[x+3]&63)<<18);
        unsigned y=1+x/4*3;out[y]=v;out[y+1]=v>>8;out[y+2]=v>>16;
    }
    return 145;
}
bool resource_picture_row_unpack(const uint8_t *in,unsigned bytes,uint8_t out[192]){
    if(!in || !out || !bytes || bytes>193)return false;
    if(in[0]==0){if(bytes!=193)return false;memcpy(out,in+1,192);return true;}
    if(in[0]==1){
        if(bytes!=145)return false;
        for(unsigned x=0;x<192;x+=4){
            unsigned y=1+x/4*3,v=in[y]|((unsigned)in[y+1]<<8)|((unsigned)in[y+2]<<16);
            out[x]=(v&63)|128;out[x+1]=((v>>6)&63)|128;
            out[x+2]=((v>>12)&63)|128;out[x+3]=((v>>18)&63)|128;
        }
        return true;
    }
    if(in[0]!=2)return false;
    unsigned x=0,at=1;
    while(at<bytes){
        unsigned token=in[at++],run=(token>>6)+1;
        if(run==4){if(at==bytes)return false;run+=in[at++];}
        if(run>192-x)return false;
        memset(out+x,(token&63)|128,run);x+=run;
    }
    return x==192;
}
