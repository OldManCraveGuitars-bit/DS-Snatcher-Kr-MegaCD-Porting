#include "resource_cdda_stream.h"
#include "system_perf.h"
#include <limits.h>
#include <string.h>

static uint32_t le32(const uint8_t *p){
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
void resource_cdda_close(resource_cdda_stream *s){
    if(!s)return;
    if(s->file)fclose(s->file);
    memset(s,0,sizeof(*s));
}
bool resource_cdda_open(resource_cdda_stream *s,const char *path,uint32_t origin){
    if(!s || !path || s->file || s->frames || s->failed)return false;
    FILE *f=fopen(path,"rb");if(!f)return false;
    uint8_t h[24];
    bool good=system_perf_fread(h,1,sizeof(h),f)==sizeof(h) &&
        !memcmp(h,"NST1\1\0\2\0",8) && le32(h+8)==32000 &&
        le32(h+16)==24 && le32(h+20)==16;
    uint32_t frames=good?le32(h+12):0;
    /* Also bound the signed cursor-distance comparison across a timer wrap. */
    good=good && frames>0 && frames<=(INT32_MAX-24u)/4u && !fseek(f,0,SEEK_END);
    if(good)good=ftell(f)==(long)(24u+frames*4u) && !fseek(f,24,SEEK_SET);
    if(!good){fclose(f);return false;}
    s->file=f;s->frames=frames;s->origin=origin;s->produced=0;return true;
}
bool resource_cdda_mix(resource_cdda_stream *s,int16_t *out,unsigned frames,
                       int16_t *scratch,unsigned capacity){
    if(!s || s->failed || !out || !scratch || !capacity)return false;
    if(!s->frames || s->produced==s->frames)return true;
    if(!s->file || s->produced>s->frames){s->failed=true;return false;}
    if(frames>s->frames-s->produced)frames=s->frames-s->produced;
    while(frames){
        unsigned n=frames>capacity?capacity:frames;
        if(system_perf_fread(scratch,4,n,s->file)!=n){s->failed=true;return false;}
        for(unsigned i=0;i<n*2;i++){
            int value=out[i]+scratch[i];
            out[i]=value>32767?32767:value< -32768?-32768:value;
        }
        out+=n*2;frames-=n;s->produced+=n;
    }
    if(s->produced==s->frames){fclose(s->file);s->file=NULL;}
    return true;
}
bool resource_cdda_position(const resource_cdda_stream *s,uint32_t played,unsigned *position){
    if(!s || !position || !s->frames || s->failed)return false;
    int32_t elapsed=(int32_t)(played-s->origin);
    if(elapsed<0)return false;
    *position=(uint32_t)elapsed>s->frames?s->frames:(uint32_t)elapsed;
    return true;
}
int resource_cdda_status(const resource_cdda_stream *s,uint32_t played){
    if(!s || s->failed)return -1;
    if(!s->frames)return 0;
    unsigned position;
    if(!resource_cdda_position(s,played,&position))return -1;
    return position<s->frames?0x100:0;
}
static unsigned bcd(unsigned n){return n/10*16+n%10;}
uint32_t resource_cdda_clock(const resource_cdda_stream *s,uint32_t played){
    unsigned position=0;
    if(!resource_cdda_position(s,played,&position))return 0;
    unsigned frames=(uint64_t)position*75/32000;
    return bcd(frames/4500)<<24|bcd(frames/75%60)<<16|bcd(frames%75)<<8;
}
