#include <stddef.h>
#include <malloc.h>
void *__real_malloc(size_t bytes);
volatile unsigned fm_failed_allocation,fm_allocation_failures;
volatile unsigned fm_heap_failure[3];
void *__wrap_malloc(size_t bytes){
 void *p=__real_malloc(bytes);
 if(!p && bytes){
  fm_failed_allocation=bytes;fm_allocation_failures++;
  struct mallinfo heap=mallinfo();
  fm_heap_failure[0]=heap.uordblks;fm_heap_failure[1]=heap.fordblks;
  fm_heap_failure[2]=heap.keepcost;
 }
 return p;
}
