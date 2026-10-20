#include "voice_buffers.h"
#include <malloc.h>
#include <stdlib.h>
volatile unsigned voice_buffer_failure[5];
static void *ring_alloc(unsigned bytes){
    void *p=memalign(32,bytes);
    if(!p){
        struct mallinfo heap=mallinfo();
        voice_buffer_failure[0]=bytes;voice_buffer_failure[1]=heap.uordblks;
        voice_buffer_failure[2]=heap.fordblks;voice_buffer_failure[3]=heap.keepcost;
        voice_buffer_failure[4]++;
    }
    return p;
}
void voice_buffers_end(voice_buffers *b){
    if(!b)return;
    free(b->pcm);if(b->right!=b->left)free(b->right);free(b->left);
    b->pcm=NULL;b->left=b->right=NULL;b->capacity=0;
}
bool voice_buffers_begin(voice_buffers *b,unsigned capacity,unsigned channels){
    if(!b || channels>2 || !capacity || capacity>65536 || capacity%32 ||
       b->pcm || b->left || b->right || b->capacity)return false;
    /* Each hardware channel must remain contiguous. Largest allocations
     * precede the original 8-bit staging chunk. */
    if(channels){
        b->left=ring_alloc(capacity*2);
        if(!b->left)return false;
        b->right=channels==1?b->left:ring_alloc(capacity*2);
        if(!b->right){voice_buffers_end(b);return false;}
    }
    b->pcm=ring_alloc(channels?VOICE_BUFFER_SCRATCH:capacity);
    if(!b->pcm){voice_buffers_end(b);return false;}
    b->capacity=capacity;return true;
}
bool voice_buffers_reserve(voice_buffers *b,unsigned preferred,unsigned minimum,unsigned channels){
    /* Power-of-two hardware rings, with at least four streaming chunks.
     * Retry only after every partial allocation has been released. This
     * changes look-ahead storage, never source samples, channels or clocks. */
    if(!b || channels>2 || minimum<8192 || preferred>65536 || minimum>preferred ||
       (minimum&(minimum-1)) || (preferred&(preferred-1)) ||
       b->pcm || b->left || b->right || b->capacity)return false;
    for(unsigned capacity=preferred;capacity>=minimum;capacity>>=1)
        if(voice_buffers_begin(b,capacity,channels))return true;
    return false;
}
