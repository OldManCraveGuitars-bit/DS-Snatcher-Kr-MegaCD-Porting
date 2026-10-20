#include "resource_music.h"
#include <string.h>
#include <stdlib.h>
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
#include <nds.h>
volatile unsigned music_bus_io_cycles,music_bus_mix_cycles;
#endif
typedef struct {FILE *file;uint8_t *buffer;unsigned offset,frames,start,position,loops;} music_bus;
struct resource_music_channels {unsigned count;music_bus bus[6];};
static unsigned read32(const uint8_t *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
void resource_music_close(resource_music *s){
    if(!s)return;
    if(s->channels){
        for(unsigned i=0;i<6;i++){
            if(s->channels->bus[i].file)fclose(s->channels->bus[i].file);
            free(s->channels->bus[i].buffer);
        }
        free(s->channels);
    }
    if(s->file)fclose(s->file);
    free(s->buffer);
    memset(s,0,sizeof(*s));
}
static bool open_channels(resource_music *s,const char *path,const uint8_t h[32],unsigned command){
    if(memcmp(h,"NSMC\1\0\2\0",8) || read32(h+8)!=32000 || read32(h+12)!=6 ||
       read32(h+16)!=command || read32(h+20)!=32 || read32(h+24)!=32 || read32(h+28))return false;
    if(fseek(s->file,0,SEEK_END))return false;
    long bytes=ftell(s->file);if(bytes<224 || bytes>512L*1024*1024 || fseek(s->file,32,SEEK_SET))return false;
    s->channels=calloc(1,sizeof(*s->channels));if(!s->channels)return false;
    s->channels->count=6;s->command=command;
    unsigned offset=224;
    for(unsigned i=0;i<6;i++){
        uint8_t row[32];music_bus *b=s->channels->bus+i;
        if(fread(row,1,32,s->file)!=32)return false;
        b->offset=read32(row);b->frames=read32(row+4);b->start=read32(row+8);
        if(b->offset!=offset || !b->frames || b->frames>32000u*1800 || b->start>=b->frames ||
           read32(row+12)!=b->frames || read32(row+16) || read32(row+20) || read32(row+24) || read32(row+28))return false;
        offset+=b->frames*4;if(offset>(unsigned)bytes)return false;
        b->file=fopen(path,"rb");b->buffer=malloc(8192);
        if(!b->file || !b->buffer || setvbuf(b->file,(char*)b->buffer,_IOFBF,8192) || fseek(b->file,b->offset,SEEK_SET))return false;
        if(b->frames>s->frames)s->frames=b->frames;
    }
    return offset==(unsigned)bytes;
}
bool resource_music_open(resource_music *s,const char *path,unsigned command){
    if(!s || !path)return false;
    resource_music_close(s);
    s->file=fopen(path,"rb");if(!s->file)return false;
    /* Read contiguous sectors in a single file refill on physical cards.
     * Small mixer chunks must not each trigger tiny default stdio reads. */
    s->buffer=malloc(8192);
    if(!s->buffer || setvbuf(s->file,(char*)s->buffer,_IOFBF,8192))goto bad;
    uint8_t h[32];
    if(fread(h,1,32,s->file)!=32)goto bad;
    if(!memcmp(h,"NSMC",4)){
        if(open_channels(s,path,h,command))return true;
        goto bad;
    }
    if(memcmp(h,"NSMU\1\0\2\0",8) || read32(h+8)!=32000 ||
       read32(h+24)!=command || read32(h+28)>1)goto bad;
    s->frames=read32(h+12);s->loop_start=read32(h+16);s->loop_end=read32(h+20);s->command=command;
    s->ended_silence=read32(h+28)==1;
    if(s->ended_silence && s->loop_start!=UINT32_MAX)goto bad;
    if(!s->frames || s->frames>32000u*1800 || s->loop_end!=s->frames || (s->loop_start!=UINT32_MAX && s->loop_start>=s->loop_end) ||
       fseek(s->file,0,SEEK_END) || ftell(s->file)!=(long)(32+s->frames*4) || fseek(s->file,32,SEEK_SET))goto bad;
    return true;
bad:
    resource_music_close(s);return false;
}
static bool read_channels(resource_music *s,int16_t *out,unsigned frames){
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
    unsigned begin=cpuGetTiming();
#endif
    unsigned total=frames;
    while(frames){
        unsigned n=frames>128?128:frames;
        int32_t sum[256]={0};int16_t pcm[256];
        for(unsigned i=0;i<s->channels->count;i++){
            music_bus *b=s->channels->bus+i;unsigned filled=0;
            while(filled<n){
                if(b->position==b->frames){
                    if(fseek(b->file,b->offset+b->start*4,SEEK_SET))goto bad_channels;
                    b->position=b->start;b->loops++;if(b->loops>s->loops)s->loops=b->loops;
                }
                unsigned take=b->frames-b->position;if(take>n-filled)take=n-filled;
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
                unsigned read_begin=cpuGetTiming();
#endif
                if(fread(pcm+filled*2,4,take,b->file)!=take)goto bad_channels;
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
                music_bus_io_cycles+=cpuGetTiming()-read_begin;
#endif
                b->position+=take;filled+=take;
            }
            for(unsigned k=0;k<n*2;k++)sum[k]+=pcm[k];
        }
        for(unsigned k=0;k<n*2;k++)out[k]=sum[k]>32767?32767:sum[k]<-32768?-32768:sum[k];
        out+=n*2;frames-=n;
    }
    s->position+=total;
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
    music_bus_mix_cycles+=cpuGetTiming()-begin;
#endif
    return true;
bad_channels:
    s->error=true;memset(out,0,frames*4);return false;
}
bool resource_music_read(resource_music *s,int16_t *out,unsigned frames){
    if(!s || !s->file || s->error || !out)return false;
    if(s->channels)return read_channels(s,out,frames);
    while(frames){
        if(s->position==s->loop_end){
            if(s->ended_silence){memset(out,0,frames*4);return true;}
            if(s->loop_start==UINT32_MAX){s->error=true;break;}
            if(fseek(s->file,32+s->loop_start*4,SEEK_SET)){s->error=true;break;}
            s->position=s->loop_start;s->loops++;
        }
        unsigned n=s->loop_end-s->position;if(n>frames)n=frames;
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
        unsigned read_begin=cpuGetTiming();
#endif
        if(fread(out,4,n,s->file)!=n){s->error=true;break;}
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
        music_bus_io_cycles+=cpuGetTiming()-read_begin;
#endif
        s->position+=n;out+=n*2;frames-=n;
    }
    if(s->error){memset(out,0,frames*4);return false;}
    return true;
}
