#include "return_audio_queue.h"
#include <string.h>
bool return_audio_queue_bind(return_audio_queue *q,int16_t *storage,unsigned capacity){
    if(!q || !storage || !capacity || capacity>RETURN_AUDIO_QUEUE_MAX_FRAMES)return false;
    q->samples=storage;q->capacity=capacity;q->read=q->count=0;return true;
}
unsigned return_audio_queue_write_view(return_audio_queue *q,int16_t **out,unsigned limit){
    if(!q || !out || !q->samples || !q->capacity)return 0;
    unsigned tail=(q->read+q->count)%q->capacity,n=q->capacity-q->count;
    if(n>q->capacity-tail)n=q->capacity-tail;
    if(n>limit)n=limit;
    *out=q->samples+tail*2;return n;
}
bool return_audio_queue_publish(return_audio_queue *q,unsigned n){
    if(!q || !q->samples || !q->capacity || n>q->capacity-q->count)return false;
    unsigned tail=(q->read+q->count)%q->capacity;
    if(n>q->capacity-tail)return false;
    q->count+=n;return true;
}
unsigned return_audio_queue_read(return_audio_queue *q,int16_t *out,unsigned n){
    if(!q || !out || !q->samples || !q->capacity)return 0;
    if(n>q->count)n=q->count;
    unsigned first=n;if(first>q->capacity-q->read)first=q->capacity-q->read;
    memcpy(out,q->samples+q->read*2,first*4);
    if(n>first)memcpy(out+first*2,q->samples,(n-first)*4);
    q->read=(q->read+n)%q->capacity;q->count-=n;return n;
}
