#include "return_audio_queue.h"
#include <string.h>
bool return_audio_queue_bind(return_audio_queue *q,int16_t *storage,unsigned capacity){
    if(!q || !storage || !capacity || capacity>RETURN_AUDIO_QUEUE_MAX_FRAMES)return false;
    q->samples=storage;q->capacity=capacity;q->read=q->count=0;q->blocks=NULL;q->block_frames=0;return true;
}
bool return_audio_queue_bind_blocks(return_audio_queue *q,int16_t *const *blocks,unsigned count,unsigned frames){
    if(!q || !blocks || !count || !frames || frames>RETURN_AUDIO_QUEUE_MAX_FRAMES ||
       count>RETURN_AUDIO_QUEUE_MAX_FRAMES/frames)return false;
    for(unsigned i=0;i<count;i++)if(!blocks[i])return false;
    q->samples=blocks[0];q->capacity=count*frames;q->read=q->count=0;
    q->blocks=blocks;q->block_frames=frames;return true;
}
unsigned return_audio_queue_write_view(return_audio_queue *q,int16_t **out,unsigned limit){
    if(!q || !out || !q->samples || !q->capacity)return 0;
    unsigned tail=(q->read+q->count)%q->capacity,n=q->capacity-q->count;
    if(n>q->capacity-tail)n=q->capacity-tail;
    if(n>limit)n=limit;
    if(q->blocks){
        unsigned index=tail/q->block_frames,offset=tail%q->block_frames;
        if(n>q->block_frames-offset)n=q->block_frames-offset;
        *out=q->blocks[index]+offset*2;
    }else *out=q->samples+tail*2;
    return n;
}
bool return_audio_queue_publish(return_audio_queue *q,unsigned n){
    if(!q || !q->samples || !q->capacity || n>q->capacity-q->count)return false;
    unsigned tail=(q->read+q->count)%q->capacity;
    if(n>q->capacity-tail || (q->blocks && n>q->block_frames-tail%q->block_frames))return false;
    q->count+=n;return true;
}
unsigned return_audio_queue_read(return_audio_queue *q,int16_t *out,unsigned n){
    if(!q || !out || !q->samples || !q->capacity)return 0;
    if(n>q->count)n=q->count;
    if(!q->blocks){
        unsigned first=n;if(first>q->capacity-q->read)first=q->capacity-q->read;
        memcpy(out,q->samples+q->read*2,first*4);
        if(n>first)memcpy(out+first*2,q->samples,(n-first)*4);
        q->read=(q->read+n)%q->capacity;
    }else{
        for(unsigned done=0;done<n;){
            unsigned offset=q->read%q->block_frames,take=q->block_frames-offset;
            if(take>n-done)take=n-done;
            memcpy(out+done*2,q->blocks[q->read/q->block_frames]+offset*2,take*4);
            done+=take;q->read=(q->read+take)%q->capacity;
        }
    }
    q->count-=n;return n;
}
