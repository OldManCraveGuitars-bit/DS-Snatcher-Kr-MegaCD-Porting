#include "resource_voice_stream.h"
#include <limits.h>
#include <string.h>
bool resource_voice_stream_begin(resource_voice_stream *s,void *buffer,unsigned capacity,
        unsigned total,resource_voice_read read,resource_voice_publish publish,void *context){
    if(!s || !buffer || !read || !publish || capacity<RESOURCE_VOICE_CHUNK*4 ||
       capacity>65536 || capacity%RESOURCE_VOICE_CHUNK || !total || total>786432 || total%4)return false;
    memset(s,0,sizeof(*s));s->buffer=buffer;s->capacity=capacity;s->total=total;
    s->read=read;s->publish=publish;s->context=context;s->min_lead=UINT_MAX;
    memset(buffer,0,capacity);
    while(s->written<capacity && s->written<total){
        unsigned n=total-s->written;if(n>RESOURCE_VOICE_CHUNK)n=RESOURCE_VOICE_CHUNK;
        if(!read(context,s->buffer+s->written,n)){s->error=1;return false;}
        s->written+=n;
    }
    s->written=capacity; /* Includes the silence already cleared after EOF. */
    publish(context,buffer,capacity);return true;
}
bool resource_voice_stream_begin_sink(resource_voice_stream *s,void *scratch,unsigned scratch_bytes,
        unsigned capacity,unsigned total,resource_voice_read read,resource_voice_sink sink,void *context){
    if(!s || !scratch || scratch_bytes<RESOURCE_VOICE_CHUNK || !read || !sink ||
       capacity<RESOURCE_VOICE_CHUNK*4 || capacity>65536 || capacity%RESOURCE_VOICE_CHUNK ||
       !total || total>786432 || total%4)return false;
    memset(s,0,sizeof(*s));s->buffer=scratch;s->capacity=capacity;s->total=total;
    s->read=read;s->sink=sink;s->context=context;s->min_lead=UINT_MAX;
    for(unsigned offset=0;offset<capacity;offset+=RESOURCE_VOICE_CHUNK){
        unsigned n=total>offset?total-offset:0;if(n>RESOURCE_VOICE_CHUNK)n=RESOURCE_VOICE_CHUNK;
        if(n && !read(context,scratch,n)){s->error=1;return false;}
        if(n<RESOURCE_VOICE_CHUNK)memset((uint8_t*)scratch+n,0,RESOURCE_VOICE_CHUNK-n);
        sink(context,offset,scratch,RESOURCE_VOICE_CHUNK);
    }
    s->written=capacity;return true;
}
bool resource_voice_stream_update(resource_voice_stream *s,unsigned played){
    if(!s || !s->buffer || s->error)return false;
    if(played<s->played){s->error=2;return false;}
    s->played=played;
    if(played>=s->total)return true;
    if(played>=s->written){s->error=3;return false;}
    unsigned lead=s->written-played;if(lead<s->min_lead)s->min_lead=lead;
    /* Keep a full chunk behind the timer cursor untouched: the sound FIFO
     * and launch message may lag the ARM9 timer. Refill at most one chunk
     * per game update; no seek, sample omission or resampling is involved. */
    if(played<RESOURCE_VOICE_GUARD || s->written>=s->total+RESOURCE_VOICE_CHUNK)return true;
    unsigned safe=played+s->capacity-RESOURCE_VOICE_GUARD;
    if(safe<s->written || safe-s->written<RESOURCE_VOICE_CHUNK)return true;
    unsigned offset=s->written%s->capacity,n=s->total>s->written?s->total-s->written:0;
    if(n>RESOURCE_VOICE_CHUNK)n=RESOURCE_VOICE_CHUNK;
    uint8_t *output=s->sink?s->buffer:s->buffer+offset;
    if(n && !s->read(s->context,output,n)){s->error=1;return false;}
    /* Silence after EOF until the existing voice timer stops channel0. */
    if(n<RESOURCE_VOICE_CHUNK)memset(output+n,0,RESOURCE_VOICE_CHUNK-n);
    if(s->sink)s->sink(s->context,offset,output,RESOURCE_VOICE_CHUNK);
    else s->publish(s->context,output,RESOURCE_VOICE_CHUNK);
    s->written+=RESOURCE_VOICE_CHUNK;s->refills++;return true;
}
