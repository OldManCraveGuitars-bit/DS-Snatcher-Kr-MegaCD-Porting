#ifndef RESOURCE_ROW_COPY_H
#define RESOURCE_ROW_COPY_H
#include <stdint.h>
/* Distinct, word-aligned 256-byte cache rows, with arbitrary subspans.
 * ARM9 memcpy falls back to byte copies when the two spans have different
 * alignment. Merge adjacent aligned words instead; all reads remain within
 * the source row. Neither the source pixels nor the copy order changes. */
static inline void resource_row_copy(uint8_t *out,const uint8_t *in,unsigned n){
    typedef uint32_t row_word __attribute__((may_alias));
    while(n && ((uintptr_t)out&3)){*out++=*in++;n--;}
    unsigned shift=((uintptr_t)in&3)*8;
    if(shift){
        if(n>=8){
            const row_word *p=(const row_word*)((uintptr_t)in&~(uintptr_t)3);
            uint32_t previous=*p++;
            do{
                uint32_t next=*p++;
                *(row_word*)out=(previous>>shift)|(next<<(32-shift));
                previous=next;out+=4;in+=4;n-=4;
            }while(n>=8);
        }
    }else while(n>=4){*(row_word*)out=*(const row_word*)in;out+=4;in+=4;n-=4;}
    while(n--)*out++=*in++;
}
#endif
