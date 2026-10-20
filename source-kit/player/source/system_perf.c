#include <nds.h>
#include <string.h>
#include "system_perf.h"
volatile unsigned system_perf_totals[PERF_COUNT];
static unsigned category;
static unsigned short last;
static int enabled;
static void account(void){
    unsigned short now=TIMER3_DATA;
    system_perf_totals[category]+=(unsigned short)(now-last);last=now;
}
void system_perf_start(void){
    int lock=enterCriticalSection();
    TIMER3_CR=0;TIMER3_DATA=0;last=category=0;
    memset((void*)system_perf_totals,0,sizeof(system_perf_totals));
    TIMER3_CR=TIMER_ENABLE|TIMER_DIV_1024;enabled=1;
    leaveCriticalSection(lock);
}
unsigned system_perf_enter(unsigned next){
    int lock=enterCriticalSection();
    unsigned previous=category;
    if(enabled){account();if(next<PERF_COUNT)category=next;}
    leaveCriticalSection(lock);return previous;
}
void system_perf_leave(unsigned previous){(void)system_perf_enter(previous);}
void system_perf_snapshot(unsigned out[PERF_COUNT]){
    int lock=enterCriticalSection();if(enabled)account();
    for(unsigned i=0;i<PERF_COUNT;i++)out[i]=system_perf_totals[i];
    leaveCriticalSection(lock);
}
void system_perf_wait_vblank(void){
    unsigned previous=system_perf_enter(PERF_WAIT);
    swiWaitForVBlank();system_perf_leave(previous);
}
size_t system_perf_fread(void *p,size_t size,size_t count,FILE *f){
    unsigned previous=system_perf_enter(PERF_READ);
    size_t result=fread(p,size,count,f);system_perf_leave(previous);return result;
}
